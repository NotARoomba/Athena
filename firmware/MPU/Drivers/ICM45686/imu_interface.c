/*
 * IMU Interface for STM32 - ICM-45686
 * Advanced API with FSYNC synchronization at 30Hz
 */

#include "imu_interface.h"
#include "main.h"
#include "inv_imu_driver_advanced.h"
#include "athena.h"
#include <stdio.h>
#include <string.h>

/* External handles */
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim1;

/* FSYNC frequency */
#define FSYNC_FREQUENCY_HZ 30

/* Static variables */
static inv_imu_device_t imu_dev;
static volatile uint8_t int1_flag = 0;
static volatile uint32_t int1_timestamp = 0;
static uint8_t fifo_data[FIFO_MIRRORING_SIZE];
static uint32_t timestamp;

/* SPI Read/Write implementation for IMU1 */
static int imu1_read_reg(uint8_t reg, uint8_t *buf, uint32_t len)
{
    uint8_t tx_buf[32];
    uint8_t rx_buf[32];
    
    if (len + 1 > sizeof(tx_buf)) {
        print("IMU1: Read length %lu too large\r\n", len);
        return -1;
    }
    
    tx_buf[0] = (uint8_t)(reg | 0x80);  // Set read bit (MSB=1 for read)
    for (uint32_t i = 1; i <= len; i++) {
        tx_buf[i] = 0x00;  // Dummy bytes to clock out data
    }
    
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_SET);  // CS active low
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, len + 1, 100);
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_RESET);  // CS inactive high
    
    if (status != HAL_OK) {
        print("IMU1: SPI TransmitReceive FAILED - reg=0x%02X, status=%d\r\n", reg, status);
        return -1;
    }
    
    // Copy received data (skip first byte which is dummy response to register address)
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = rx_buf[i + 1];
    }
    
    return 0;
}

static int imu1_write_reg(uint8_t reg, const uint8_t *buf, uint32_t len)
{
    uint8_t tx_buf[32];
    
    if (len + 1 > sizeof(tx_buf)) {
        print("IMU1: Write length %lu too large\r\n", len);
        return -1;
    }
    
    tx_buf[0] = (uint8_t)(reg & 0x7F);  // Clear read bit (MSB=0 for write)
    for (uint32_t i = 0; i < len; i++) {
        tx_buf[i + 1] = buf[i];
    }
    
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_SET);  // CS active low
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, tx_buf, len + 1, 100);
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_RESET);  // CS inactive high
    
    if (status != HAL_OK) {
        print("IMU1: SPI write FAILED - reg=0x%02X, status=%d\r\n", reg, status);
        return -1;
    }
    
    return 0;
}

static void imu1_sleep_us(uint32_t us)
{
    uint32_t start = GetTimestamp();
    while ((GetTimestamp() - start) < us);
}

/* Sensor event callback - called when FIFO data is parsed */
static void sensor_event_cb(inv_imu_sensor_event_t *event)
{
    static uint32_t event_fsync_ts = 0;
    static uint16_t event_fsync_delay_cnt = 0;
    
    /* Check for FSYNC event */
    if (event->sensor_mask & (1 << INV_SENSOR_FSYNC_EVENT)) {
        event->timestamp_fsync = (uint16_t)((uint64_t)event->timestamp_fsync *
                                            inv_imu_adv_get_timestamp_resolution_us(&imu_dev));
        event_fsync_ts = timestamp;
        event_fsync_delay_cnt = event->timestamp_fsync;
    }
    
    /* Process gyro data */
    if (event->sensor_mask & (1 << INV_SENSOR_GYRO)) {
        float gyro_dps[3];
        
        /* Convert to SI units (FSR: 2000dps, 16-bit signed) */
        gyro_dps[0] = (float)(event->gyro[0] * 2000.0) / 32768.0;
        gyro_dps[1] = (float)(event->gyro[1] * 2000.0) / 32768.0;
        gyro_dps[2] = (float)(event->gyro[2] * 2000.0) / 32768.0;
        
        /* Print with FSYNC tag if this data coincides with FSYNC event */
        if (timestamp == event_fsync_ts) {
            print("%10lu us   Gyro[% 7.1f % 7.1f % 7.1f]dps   FSYNC event %hu us\r\n",
                  timestamp, gyro_dps[0], gyro_dps[1], gyro_dps[2], event_fsync_delay_cnt);
        } else {
            print("%10lu us   Gyro[% 7.1f % 7.1f % 7.1f]dps\r\n",
                  timestamp, gyro_dps[0], gyro_dps[1], gyro_dps[2]);
        }
    }
    
    /* Process accel data */
    if (event->sensor_mask & (1 << INV_SENSOR_ACCEL)) {
        float accel_g[3];
        
        /* Convert to SI units (FSR: 4g, 16-bit signed) */
        accel_g[0] = (float)(event->accel[0] * 4.0) / 32768.0;
        accel_g[1] = (float)(event->accel[1] * 4.0) / 32768.0;
        accel_g[2] = (float)(event->accel[2] * 4.0) / 32768.0;
        
        print("%10lu us   Accel[% 6.2f % 6.2f % 6.2f]g\r\n",
              timestamp, accel_g[0], accel_g[1], accel_g[2]);
    }
}

/* Initialize IMU1 with FSYNC */
int IMU_Init(void)
{
    int rc = 0;
    inv_imu_int_pin_config_t int_pin_config;
    inv_imu_int_state_t int_config;
    inv_imu_adv_fifo_config_t fifo_config;
    inv_imu_adv_var_t *e = (inv_imu_adv_var_t *)imu_dev.adv_var;
    
    print("IMU1: Starting advanced initialization with FSYNC...\r\n");
    
    /* Init transport layer */
    imu_dev.transport.read_reg   = imu1_read_reg;
    imu_dev.transport.write_reg  = imu1_write_reg;
    imu_dev.transport.serif_type = UI_SPI4;
    imu_dev.transport.sleep_us   = imu1_sleep_us;
    
    /* Init sensor event callback */
    e->sensor_event_cb = sensor_event_cb;
    
    /* Configure SPI slew rate */
    drive_config0_t drive_config0;
    drive_config0.pads_spi_slew = DRIVE_CONFIG0_PADS_SPI_SLEW_TYP_10NS;
    rc = inv_imu_write_reg(&imu_dev, DRIVE_CONFIG0, 1, (uint8_t *)&drive_config0);
    if (rc != 0) {
        print("IMU1: Failed to configure SPI slew rate\r\n");
        return rc;
    }
    imu1_sleep_us(2);
    
    /* Initialize with advanced API (handles WHO_AM_I check and soft reset) */
    print("IMU1: Calling inv_imu_adv_init...\r\n");
    rc = inv_imu_adv_init(&imu_dev);
    if (rc != 0) {
        print("IMU1: inv_imu_adv_init FAILED with code %d\r\n", rc);
        return rc;
    }
    print("IMU1: Advanced init successful\r\n");
    
    /* Configure interrupt pin: Polarity High, Pulse mode, Push-Pull */
    int_pin_config.int_polarity = INTX_CONFIG2_INTX_POLARITY_HIGH;
    int_pin_config.int_mode = INTX_CONFIG2_INTX_MODE_PULSE;
    int_pin_config.int_drive = INTX_CONFIG2_INTX_DRIVE_PP;
    rc = inv_imu_set_pin_config_int(&imu_dev, INV_IMU_INT1, &int_pin_config);
    if (rc != 0) {
        print("IMU1: Failed to configure INT pin\r\n");
        return rc;
    }
    
    /* Configure FSYNC on INT2 pin (PE9 is wired to FSYNC/INT2) */
    print("IMU1: Configuring FSYNC...\r\n");
    rc = inv_imu_adv_set_int2_pin_usage(&imu_dev, IOC_PAD_SCENARIO_OVRD_INT2_CFG_OVRD_VAL_FSYNC);
    if (rc != 0) {
        print("IMU1: Failed to set INT2 to FSYNC mode\r\n");
        return rc;
    }
    
    rc = inv_imu_adv_enable_fsync(&imu_dev);
    if (rc != 0) {
        print("IMU1: Failed to enable FSYNC\r\n");
        return rc;
    }
    print("IMU1: FSYNC enabled\r\n");
    
    /* Configure interrupts for FIFO and FSYNC */
    memset(&int_config, INV_IMU_DISABLE, sizeof(int_config));
    int_config.INV_FIFO_THS = INV_IMU_ENABLE;
    int_config.INV_UI_FSYNC = INV_IMU_ENABLE;
    rc = inv_imu_set_config_int(&imu_dev, INV_IMU_INT1, &int_config);
    if (rc != 0) {
        print("IMU1: Failed to configure interrupts\r\n");
        return rc;
    }
    
    /* Configure FIFO with FSYNC tagging */
    print("IMU1: Configuring FIFO with FSYNC tagging...\r\n");
    rc = inv_imu_adv_get_fifo_config(&imu_dev, &fifo_config);
    fifo_config.base_conf.fifo_mode = FIFO_CONFIG0_FIFO_MODE_SNAPSHOT;
    fifo_config.base_conf.gyro_en = 1;
    fifo_config.base_conf.accel_en = 1;
    fifo_config.base_conf.hires_en = 0;
    fifo_config.base_conf.fifo_wm_th = 1;  // Interrupt on every sample
    fifo_config.tmst_fsync_en = INV_IMU_ENABLE;  // Enable FSYNC timestamp
    fifo_config.fifo_wr_wm_gt_th = FIFO_CONFIG2_FIFO_WR_WM_EQ_OR_GT_TH;
    rc = inv_imu_adv_set_fifo_config(&imu_dev, &fifo_config);
    if (rc != 0) {
        print("IMU1: Failed to configure FIFO\r\n");
        return rc;
    }
    
    /* Disable FSYNC AP tagging (use FIFO tagging instead) */
    rc = inv_imu_adv_configure_fsync_ap_tag(&imu_dev, FSYNC_CONFIG0_AP_FSYNC_NO);
    if (rc != 0) {
        print("IMU1: Failed to configure FSYNC AP tag\r\n");
        return rc;
    }
    
    /* Set FSR: Accel=4g, Gyro=2000dps */
    print("IMU1: Configuring FSR...\r\n");
    rc = inv_imu_set_accel_fsr(&imu_dev, ACCEL_CONFIG0_ACCEL_UI_FS_SEL_4_G);
    if (rc != 0) {
        print("IMU1: Failed to set accel FSR\r\n");
        return rc;
    }
    
    rc = inv_imu_set_gyro_fsr(&imu_dev, GYRO_CONFIG0_GYRO_UI_FS_SEL_2000_DPS);
    if (rc != 0) {
        print("IMU1: Failed to set gyro FSR\r\n");
        return rc;
    }
    
    /* Set ODR: 200 Hz (to capture 30Hz FSYNC with margin) */
    print("IMU1: Configuring ODR to 200 Hz...\r\n");
    rc = inv_imu_set_accel_frequency(&imu_dev, ACCEL_CONFIG0_ACCEL_ODR_200_HZ);
    if (rc != 0) {
        print("IMU1: Failed to set accel ODR\r\n");
        return rc;
    }
    
    rc = inv_imu_set_gyro_frequency(&imu_dev, GYRO_CONFIG0_GYRO_ODR_200_HZ);
    if (rc != 0) {
        print("IMU1: Failed to set gyro ODR\r\n");
        return rc;
    }
    
    /* Set BW = ODR/4 */
    rc = inv_imu_set_accel_ln_bw(&imu_dev, IPREG_SYS2_REG_131_ACCEL_UI_LPFBW_DIV_4);
    if (rc != 0) {
        print("IMU1: Failed to set accel BW\r\n");
        return rc;
    }
    
    rc = inv_imu_set_gyro_ln_bw(&imu_dev, IPREG_SYS1_REG_172_GYRO_UI_LPFBW_DIV_4);
    if (rc != 0) {
        print("IMU1: Failed to set gyro BW\r\n");
        return rc;
    }
    
    /* Enable sensors in Low-Noise mode */
    print("IMU1: Enabling sensors in Low-Noise mode...\r\n");
    rc = inv_imu_adv_enable_accel_ln(&imu_dev);
    if (rc != 0) {
        print("IMU1: Failed to enable accel\r\n");
        return rc;
    }
    
    rc = inv_imu_adv_enable_gyro_ln(&imu_dev);
    if (rc != 0) {
        print("IMU1: Failed to enable gyro\r\n");
        return rc;
    }
    
    /* Wait for sensor startup, then flush FIFO */
    imu1_sleep_us(GYR_STARTUP_TIME_US);
    rc = inv_imu_flush_fifo(&imu_dev);
    if (rc != 0) {
        print("IMU1: Failed to flush FIFO\r\n");
        return rc;
    }
    
    /* Start FSYNC PWM at 30Hz */
    print("IMU1: Starting FSYNC PWM at %d Hz...\r\n", FSYNC_FREQUENCY_HZ);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    
    print("IMU1: Initialization complete! Waiting for interrupts...\r\n");
    return 0;
}

/* Process IMU interrupts and FIFO data */
void IMU_Process(void)
{
    /* Check if interrupt flag is set */
    if (!int1_flag) {
        return;
    }
    
    int rc;
    inv_imu_int_state_t int_state;
    uint16_t fifo_count;
    
    /* Clear interrupt flag and capture timestamp */
    __disable_irq();
    int1_flag = 0;
    timestamp = int1_timestamp;
    __enable_irq();
    
    /* Read interrupt status */
    rc = inv_imu_get_int_status(&imu_dev, INV_IMU_INT1, &int_state);
    if (rc != 0) {
        print("IMU1: Failed to read interrupt status\r\n");
        return;
    }
    
    /* Process FIFO data if threshold interrupt */
    if (int_state.INV_FIFO_THS) {
        rc = inv_imu_adv_get_data_from_fifo(&imu_dev, fifo_data, &fifo_count);
        if (rc != 0) {
            print("IMU1: Failed to read FIFO data\r\n");
            return;
        }
        
        /* Parse FIFO data - this will call sensor_event_cb for each sample */
        rc = inv_imu_adv_parse_fifo_data(&imu_dev, fifo_data, fifo_count);
        if (rc != 0) {
            print("IMU1: Failed to parse FIFO data\r\n");
            return;
        }
    }
}

/* IMU1 interrupt callback - called from EXTI handler */
void IMU1_INT_Callback(void)
{
    int1_timestamp = GetTimestamp();
    int1_flag = 1;
}
