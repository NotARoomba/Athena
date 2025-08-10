# August 9: Planning

I wanted a general purpouse flight computer that has at least 6 pyro channels, 6 PWM channels (2 TVC, 4 Fins), GNSS (bluetooth maybe), LoRa for telemetry to ground, 7.4v LiPo battery, buck converter to 5v and 3.3v. I wanted this to be the most overkill possible so I added 3 MCU's with various tasks.

IMU:

- ICM-45686 (or 3x these)

Magnetometer: LIS2MDLTR

Barometer:

- ICP-20100
- BMP388

Storage:

- SD Card
- Winbond W25Q256JV

CAN Tranciever: TCAN1051HGV-Q1

Current Sensor: INA3221

USB Hub: TUSB2036

GNSS: NEO-M8U-06B

MCU:

- STM32H753VIT6 (MPU, Sensors/Kalmann Filter)
- STM32H743VIT6 (LPU, LoRa/SD/Flash)
- STM32G474RET6 (SPU, Pyro/PWM)

Battery Charger: BQ25703ARSNR

USB-C PD: TPS25750

Servo Buck: AP64501SP-13

3.3v Buck: TPS5430
