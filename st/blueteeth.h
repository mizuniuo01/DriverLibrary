#ifndef __BLUETEETH_H
#define __BLUETEETH_H

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "usart.h"

// 缓冲区大小
#define BLUETEETH_DMA_RX_BUF_SIZE 128  // DMA单次接收最大缓存量
#define BLUETEETH_DMA_TX_BUF_SIZE 128  // DMA单次发送最大缓存量
#define BLUETEETH_RX_FIFO_SIZE    512  // 接收队列的最大容量
#define BLUETEETH_TX_FIFO_SIZE    512  // 发送队列的最大容量
#define BLUETEETH_MAX_FRAME_LEN   128  // 允许的最大单帧数据长度
#define CMD_TABLE_SIZE (sizeof(cmdTable) / sizeof(cmdTable[0])) // 动态调整的指令字典大小

// 协议定义
#define FRAME_HEADER '@'
#define FRAME_TAIL   '#'

// 状态机定义
typedef enum 
{
    STATE_WAIT_HEADER = 0,
    STATE_RECEIVING_DATA
} FrameState_t;

// 蓝牙指令字典结构体
typedef struct 
{
    const char *cmdString;
    void (*cmdHandler)(void);
} CommandMap_Struct;

// 蓝牙核心数据结构体
typedef struct
{
    UART_HandleTypeDef *huart;                       // 串口句柄
    
    uint8_t dma_rx_buffer[BLUETEETH_DMA_RX_BUF_SIZE];// DMA接收缓存
    uint8_t rxFifo[BLUETEETH_RX_FIFO_SIZE];          // 接收FIFO
    uint16_t rx_write_pos;                           // 接收写指针
    uint16_t rx_read_pos;                            // 接收读指针
    
    uint8_t dma_tx_buffer[BLUETEETH_DMA_TX_BUF_SIZE];// DMA发送缓存
    uint8_t txFifo[BLUETEETH_TX_FIFO_SIZE];          // 发送FIFO
    uint16_t tx_write_pos;                           // 发送写指针
    uint16_t tx_read_pos;                            // 发送读指针
    volatile uint8_t is_tx_busy;                     // 发送忙标志位
    
    FrameState_t rxState;                            // 接收状态机状态
    uint8_t frame_buffer[BLUETEETH_MAX_FRAME_LEN];   // 帧数据缓存
    uint16_t frameIndex;                             // 帧当前索引
} Blueteeth_Func_Struct;

// 函数声明
void Blueteeth_Init(UART_HandleTypeDef *huart);
void Blueteeth_Printf_DMA(const char *format, ...);
void Blueteeth_Display(int x, int y, const char *format, ...);
void Blueteeth_Clear(void);
void Blueteeth_Plot(const char *format, ...);
void Blueteeth_TxCpltCallback(UART_HandleTypeDef *huart);
void Blueteeth_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void Blueteeth_Task(void);

#endif