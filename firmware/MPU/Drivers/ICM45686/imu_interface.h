/*
 * IMU Interface Header for STM32 - ICM-45686
 * Advanced API with FSYNC synchronization
 */

#ifndef IMU_INTERFACE_H
#define IMU_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Initialize IMU1 with FSYNC */
int IMU_Init(void);

/* Process IMU1 interrupts and FIFO (call from main loop) */
void IMU_Process(void);

/* IMU1 interrupt callback (call from EXTI handler) */
void IMU1_INT_Callback(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_INTERFACE_H */
