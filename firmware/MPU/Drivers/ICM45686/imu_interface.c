/*
 * IMU Interface for STM32 - ICM-45686
 * Simple polling-based interface adapted from InvenSense example
 */

#include "imu_interface.h"
#include "main.h"
#include "inv_imu_driver.h"
#include "athena.h"
#include <stdio.h>
#include <string.h>

/* External SPI handle */
extern SPI_HandleTypeDef hspi1;

/* Static variables */
static inv_imu_device_t imu_dev;

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
    
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_SET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, len + 1, 100);
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_RESET);
    
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
    
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_SET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, tx_buf, len + 1, 100);
    HAL_GPIO_WritePin(IMU1_CS_GPIO_Port, IMU1_CS_Pin, GPIO_PIN_RESET);
    
    if (status != HAL_OK) {
        print("IMU1: SPI write FAILED - reg=0x%02X, status=%d\r\n", reg, status);
        return -1;
    }
    
    return 0;
}

static void imu1_sleep_us(uint32_t us)
{
    HAL_Delay((us + 999) / 1000);  // Convert microseconds to milliseconds, rounding up
}

/* Initialize IMU1 */
int IMU_Init(void)
{
    int rc = 0;
    uint8_t whoami = 0;
    
    print("IMU1: Starting initialization...\r\n");
    
    /* Init transport layer */
    imu_dev.transport.read_reg   = imu1_read_reg;
    imu_dev.transport.write_reg  = imu1_write_reg;
    imu_dev.transport.serif_type = UI_SPI4;
    imu_dev.transport.sleep_us   = imu1_sleep_us;
    
    /* Wait 3 ms to ensure device is properly supplied */
    imu1_sleep_us(3000);
    
    /* Configure SPI slew rate */
    drive_config0_t drive_config0;
    drive_config0.pads_spi_slew = DRIVE_CONFIG0_PADS_SPI_SLEW_TYP_10NS;
    rc = inv_imu_write_reg(&imu_dev, DRIVE_CONFIG0, 1, (uint8_t *)&drive_config0);
    if (rc != 0) {
        print("IMU1: Failed to configure SPI slew rate\r\n");
        return rc;
    }
    imu1_sleep_us(2); /* Takes effect 1.5 us after programmed */
    
    /* Check WHO_AM_I */
    rc = inv_imu_get_who_am_i(&imu_dev, &whoami);
    if (rc != 0) {
        print("IMU1: Failed to read WHO_AM_I\r\n");
        return rc;
    }
    print("IMU1: WHO_AM_I = 0x%02X (expected 0x%02X)\r\n", whoami, INV_IMU_WHOAMI);
    
    if (whoami != INV_IMU_WHOAMI) {
        print("IMU1: ERROR - Wrong WHO_AM_I value!\r\n");
        // return -1;
    }
    
    /* Soft reset */
    print("IMU1: Performing soft reset...\r\n");
    rc = inv_imu_soft_reset(&imu_dev);
    if (rc != 0) {
        print("IMU1: Soft reset failed\r\n");
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
    
    /* Set ODR: 50 Hz */
    print("IMU1: Configuring ODR to 50 Hz...\r\n");
    rc = inv_imu_set_accel_frequency(&imu_dev, ACCEL_CONFIG0_ACCEL_ODR_50_HZ);
    if (rc != 0) {
        print("IMU1: Failed to set accel ODR\r\n");
        return rc;
    }
    
    rc = inv_imu_set_gyro_frequency(&imu_dev, GYRO_CONFIG0_GYRO_ODR_50_HZ);
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
    rc = inv_imu_set_accel_mode(&imu_dev, PWR_MGMT0_ACCEL_MODE_LN);
    if (rc != 0) {
        print("IMU1: Failed to enable accel\r\n");
        return rc;
    }
    
    rc = inv_imu_set_gyro_mode(&imu_dev, PWR_MGMT0_GYRO_MODE_LN);
    if (rc != 0) {
        print("IMU1: Failed to enable gyro\r\n");
        return rc;
    }
    
    print("IMU1: Initialization complete!\r\n");
    return 0;
}

/* Read and print IMU1 data */
void IMU_Process(void)
{
    static uint32_t last_print = 0;
    uint32_t now = GetTimestamp();
    
    /* Print data every 100ms (10 Hz) */
    if (now - last_print < 100) {
        return;
    }
    last_print = now;
    
    int rc;
    inv_imu_sensor_data_t data;
    float accel_g[3];
    float gyro_dps[3];
    float temp_degc;
    
    /* Read sensor data from registers */
    rc = inv_imu_get_register_data(&imu_dev, &data);
    if (rc != 0) {
        print("IMU1: Failed to read data\r\n");
        return;
    }
    
    /* Check for invalid data */
    if (data.accel_data[0] == INVALID_VALUE_FIFO || 
        data.gyro_data[0] == INVALID_VALUE_FIFO) {
        return;  /* Skip invalid samples */
    }
    
    /* Convert to SI units (FSR: 4g, 2000dps, 16-bit signed) */
    accel_g[0] = (float)(data.accel_data[0] * 4.0) / 32768.0;
    accel_g[1] = (float)(data.accel_data[1] * 4.0) / 32768.0;
    accel_g[2] = (float)(data.accel_data[2] * 4.0) / 32768.0;
    
    gyro_dps[0] = (float)(data.gyro_data[0] * 2000.0) / 32768.0;
    gyro_dps[1] = (float)(data.gyro_data[1] * 2000.0) / 32768.0;
    gyro_dps[2] = (float)(data.gyro_data[2] * 2000.0) / 32768.0;
    
    temp_degc = 25.0 + ((float)data.temp_data / 128.0);
    
    /* Print data */
    print("IMU1: Accel[% 6.2f % 6.2f % 6.2f]g  Gyro[% 7.1f % 7.1f % 7.1f]dps  Temp[% 4.1f]C\r\n",
          accel_g[0], accel_g[1], accel_g[2],
          gyro_dps[0], gyro_dps[1], gyro_dps[2],
          temp_degc);
}
