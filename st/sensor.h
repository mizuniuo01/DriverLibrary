#ifndef __SENSOR_H
#define __SENSOR_H

#include "main.h"

// 传感器 I2C 7位硬件地址
#define SENSOR_I2C_ADDR_7BIT   0x4C
// 地址左移一位
#define SENSOR_I2C_ADDR        (SENSOR_I2C_ADDR_7BIT << 1)
// 传感器 I2C 通信命令符
#define SENSOR_CMD_READ_DIG    0xDD

// 传感器状态枚举
typedef enum {
    SENSOR_IDLE = 0,
    SENSOR_BUSY
} Sensor_State_t;

extern volatile uint8_t sensor_tick_flag;

void Sensor_Init(I2C_HandleTypeDef *hi2c);
Sensor_State_t Sensor_GetState(void);
void Sensor_RequestData_DMA(void);
void Sensor_RxCpltCallback(I2C_HandleTypeDef *hi2c);
void Sensor_ErrorCallback(I2C_HandleTypeDef *hi2c);
uint8_t Sensor_ReadData(void);
void Sensor_Task(void);

#endif