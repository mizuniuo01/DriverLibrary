#ifndef __GYROSCOPE_H
#define __GYROSCOPE_H

#include "header.h"

#define GYRO_DMA_RX_BUF_SIZE 128
#define GYRO_RX_FIFO_SIZE    512
#define GYRO_FRAME_HEADER    0x55
#define GYRO_TYPE_ANGLE      0x53
#define GYRO_FRAME_LEN       11

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
    UART_Regs *huart;        
    
    uint8_t dma_rx_buffer[GYRO_DMA_RX_BUF_SIZE]; // 恢复 DMA 缓冲区
    uint8_t rxFifo[GYRO_RX_FIFO_SIZE];
    volatile uint16_t rx_write_pos;
    volatile uint16_t rx_read_pos;
    
    GyroFrameState_t rxState;
    uint8_t frame_buffer[GYRO_FRAME_LEN];
    uint16_t frameIndex;
} Gyro_Func_Struct;

extern Gyro_Data_Struct gyroData;
extern volatile uint8_t gyro_checkIdleFlag;

void Gyro_Init(UART_Regs *huart);
void Gyro_RxEventCallback(UART_Regs *huart, uint16_t Size);
void Gyro_ErrorCallback(UART_Regs *huart);
void Gyro_Task(void);

#endif