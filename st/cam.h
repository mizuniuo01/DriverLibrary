#ifndef __CAM_H
#define __CAM_H

#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "usart.h"

// 缓冲区大小
#define CAM_DMA_RX_BUF_SIZE 128  // DMA单次接收最大缓存量
#define CAM_RX_FIFO_SIZE    512  // 接收队列的最大容量
#define CAM_MAX_FRAME_LEN   128  // 允许的最大单帧数据长度

// 协议定义
#define CAM_FRAME_HEADER 0xFF
#define CAM_FRAME_TAIL   0xFE

// 状态机定义
typedef enum 
{
    CAM_STATE_WAIT_HEADER = 0,
    CAM_STATE_RECEIVING_DATA
} CamFrameState_t;

// 数据结构体
typedef struct
{
    // 这里根据实际协议定义进行填充
} Cam_Data_Struct;

// 摄像头核心数据结构体
typedef struct 
{
    UART_HandleTypeDef *huart;                     // UART句柄                       
    
    uint8_t dma_rx_buffer[CAM_DMA_RX_BUF_SIZE];    // DMA接收缓冲区
    uint8_t rxFifo[CAM_RX_FIFO_SIZE];              // 接收FIFO环形缓冲区
    uint16_t rx_write_pos;                         // 写指针
    uint16_t rx_read_pos;                          // 读指针
    
    CamFrameState_t rxState;                       // 当前接收状态
    uint8_t frame_buffer[CAM_MAX_FRAME_LEN];       // 当前帧数据缓存
    uint16_t frameIndex;                           // 当前帧数据索引
} Cam_Func_Struct;

extern Cam_Data_Struct camData;
extern volatile uint8_t cam_frame_ready;

// 函数声明
void Cam_Init(UART_HandleTypeDef *huart);
void Cam_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void Cam_Task(void);

#endif