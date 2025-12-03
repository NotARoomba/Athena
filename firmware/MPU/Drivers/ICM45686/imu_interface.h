/*
 * IMU Interface Header for STM32 - ICM-45686
 * Simple polling-based interface for IMU1
 */

#ifndef IMU_INTERFACE_H
#define IMU_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Initialize IMU1 */
int IMU_Init(void);

/* Read and print IMU1 data (call from main loop) */
void IMU_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_INTERFACE_H */
