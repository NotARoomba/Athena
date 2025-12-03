
/* VERSION 1*/
#ifndef __ATHENA_H
#define __ATHENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Sensor data structures
typedef struct {
    float temperature_c;
    float pressure_pa;
    uint32_t timestamp;
} BMP388_Data;

typedef struct {
    float temperature_c;
    float pressure_pa;
    uint32_t timestamp;
} ICP201_Data;

typedef struct {
    float gyro_dps[3];         // Gyro data in degrees per second [x, y, z]
    float accel_g[3];          // Accel data in g [x, y, z]
    float temperature_c;       // Temperature in Celsius
    uint32_t timestamp;        // Timestamp in microseconds
    uint16_t fsync_tag;        // FSYNC tag/counter from IMU
    uint8_t fsync_event;       // Flag indicating FSYNC event occurred
} IMU_Data;

typedef struct {
    BMP388_Data bmp388;
    ICP201_Data icp201;
    IMU_Data imu1;
    IMU_Data imu2;
    IMU_Data imu3;
} Athena_SensorData;

typedef enum {
    LED_OFF = 0,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_YELLOW,
    LED_CYAN,
    LED_MAGENTA,
    LED_WHITE
} LED_ColorTypeDef;



typedef struct  {
    void* port_r;
    uint16_t pin_r;
    void* port_g;
    uint16_t pin_g;
    void* port_b;
    uint16_t pin_b;
} Athena_LED_PinConfig;

typedef struct {
    void* htim;   // Timer handle for millisecond counter
} Athena_TimerConfig;

void Athena_Init(Athena_LED_PinConfig* led_config, Athena_TimerConfig* timer_config);
uint32_t GetTimestamp(void);
void Set_LED_Color(LED_ColorTypeDef color);
void LED_Test_Sequence(void);
void print(const char* format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* __ATHENA_H */