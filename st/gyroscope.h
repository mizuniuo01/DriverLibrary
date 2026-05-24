#ifndef __GYROSCOPE_H
#define __GYROSCOPE_H

#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>

#define GYRO_DMA_RX_BUF_SIZE 128
#define GYRO_RX_FIFO_SIZE    512
#define GYRO_FRAME_HEADER    0x55
#define GYRO_TYPE_ANGLE      0x53
#define GYRO_FRAME_LEN       11
#define GYRO_MAX_DELTA_DEG   50.0f

typedef enum 
{
    GYRO_STATE_WAIT_HEADER = 0,
    GYRO_STATE_WAIT_TYPE,
    GYRO_STATE_RECEIVING_DATA
} GyroFrameState_t;

typedef struct
{
    float roll;
    float pitch;
    float yaw;
} Gyro_Data_Struct;

typedef struct 
{
    UART_HandleTypeDef *huart;
    
    uint8_t dma_rx_buffer[GYRO_DMA_RX_BUF_SIZE];
    uint8_t rxFifo[GYRO_RX_FIFO_SIZE];
    uint16_t rx_write_pos;
    uint16_t rx_read_pos;
    
    GyroFrameState_t rxState;
    uint8_t frame_buffer[GYRO_FRAME_LEN];
    uint16_t frameIndex;
} Gyro_Func_Struct;

extern volatile uint8_t gyro_tick_flag;
extern Gyro_Data_Struct gyroData;

void Gyro_Init(UART_HandleTypeDef *huart);
void Gyro_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void Gyro_Task(void);
void Gyro_UpdateDelta(void);
void Gyro_On_Camera_Frame(void);
float Gyro_Get_DeltaYaw(void);
void Gyro_ErrorCallback(UART_HandleTypeDef *huart);

#endif