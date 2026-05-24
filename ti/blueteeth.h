#ifndef __BLUETEETH_H
#define __BLUETEETH_H

#include "header.h"

// 缓冲区大小
#define BLUETEETH_DMA_RX_BUF_SIZE 128  
#define BLUETEETH_DMA_TX_BUF_SIZE 128  
#define BLUETEETH_RX_FIFO_SIZE    1024  
#define BLUETEETH_TX_FIFO_SIZE    1024 
#define BLUETEETH_MAX_FRAME_LEN   128  
#define CMD_TABLE_SIZE (sizeof(cmdTable) / sizeof(cmdTable[0]))

#define FRAME_HEADER '@'
#define FRAME_TAIL   '#'

typedef enum 
{
    STATE_WAIT_HEADER = 0,
    STATE_RECEIVING_DATA
} FrameState_t;

typedef struct 
{
    const char *cmdString;
    void (*cmdHandler)(void);
} CommandMap_Struct;

typedef struct
{
    UART_Regs *huart;                       
    
    uint8_t dma_rx_buffer[BLUETEETH_DMA_RX_BUF_SIZE];
    uint8_t rxFifo[BLUETEETH_RX_FIFO_SIZE];
    volatile uint16_t rx_write_pos;
    volatile uint16_t rx_read_pos;

    uint8_t dma_tx_buffer[BLUETEETH_DMA_TX_BUF_SIZE];
    uint8_t txFifo[BLUETEETH_TX_FIFO_SIZE];
    volatile uint16_t tx_write_pos;
    volatile uint16_t tx_read_pos;
    volatile uint8_t is_tx_busy;                     
    
    FrameState_t rxState;                            
    uint8_t frame_buffer[BLUETEETH_MAX_FRAME_LEN];   
    uint16_t frameIndex;                             
} Blueteeth_Func_Struct;

extern volatile uint8_t blueteeth_checkIdleFlag;

void Blueteeth_Init(UART_Regs *huart);
void Blueteeth_Printf_DMA(const char *format, ...);
void Blueteeth_Display(int x, int y, const char *format, ...);
void Blueteeth_Clear(void);
void Blueteeth_Plot(const char *format, ...);
void Blueteeth_TxCpltCallback(UART_Regs *huart);
void Blueteeth_RxEventCallback(UART_Regs *huart, uint16_t Size);
void Blueteeth_ErrorCallback(UART_Regs *huart);
void Blueteeth_Task(void);

#endif