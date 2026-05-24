#ifndef __CAM_H
#define __CAM_H

#include "header.h"

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
    // 根据实际数据格式定义解析后的数据结构
} Cam_Data_Struct;

// 摄像头核心数据结构体
typedef struct 
{
    UART_Regs *huart;                              // UART句柄                       
    
    uint8_t dma_rx_buffer[CAM_DMA_RX_BUF_SIZE];    // DMA接收缓冲区
    uint8_t rxFifo[CAM_RX_FIFO_SIZE];              // 接收FIFO环形缓冲区
    volatile uint16_t rx_write_pos;                         // 写指针
    volatile uint16_t rx_read_pos;                          // 读指针
    
    CamFrameState_t rxState;                       // 当前接收状态
    uint8_t frame_buffer[CAM_MAX_FRAME_LEN];       // 当前帧数据缓存
    uint16_t frameIndex;                           // 当前帧数据索引
} Cam_Func_Struct;

extern volatile Cam_Data_Struct camData;

// 函数声明
void Cam_Init(UART_Regs *huart);
void Cam_RxEventCallback(UART_Regs *huart, uint16_t Size);
void Cam_ErrorCallback(UART_Regs *huart);
void Cam_Task(void);

#endif