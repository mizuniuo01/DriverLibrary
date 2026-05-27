#ifndef CAM_H
#define CAM_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 缓冲区与帧参数 */
typedef enum {
    CAM_DMA_RX_BUF_SIZE = 128, /* DMA 单次接收最大缓存量 */
    CAM_RX_FIFO_SIZE = 512,    /* 接收环形队列容量 */
    CAM_MAX_FRAME_LEN = 128,   /* 单帧最大长度 */
} cam_buf_size_t;

/* 帧定界字节 */
typedef enum {
    CAM_FRAME_TAIL = 0xFE,
    CAM_FRAME_HEADER = 0xFF,
} cam_frame_byte_t;

/* 帧解析状态 */
typedef enum {
    CAM_STATE_WAIT_HEADER = 0,
    CAM_STATE_RECEIVING_DATA,
} cam_frame_state_t;

/* 摄像头句柄 */
typedef struct {
    UART_Regs *huart; /* 绑定的串口 */

    uint8_t dma_rx_buffer[CAM_DMA_RX_BUF_SIZE]; /* DMA 接收缓冲 */
    uint8_t rx_fifo[CAM_RX_FIFO_SIZE];          /* 接收环形队列 */
    volatile uint16_t rx_write_pos;             /* FIFO 写指针（ISR 写入） */
    volatile uint16_t rx_read_pos;              /* FIFO 读指针 */

    cam_frame_state_t rx_state;           /* 当前解析状态 */
    uint8_t frame_buffer[CAM_MAX_FRAME_LEN]; /* 帧组装缓冲 */
    uint16_t frame_index;                 /* 帧缓冲写入位置 */
} cam_handle_t;

/* 摄像头解析数据（根据实际数据格式定义） */
typedef struct {
    uint8_t reserved[32];
} cam_data_t;

void cam_init(UART_Regs *huart);
void cam_rx_callback(UART_Regs *huart, uint16_t size);
void cam_error_callback(UART_Regs *huart);
void cam_task(void);
cam_data_t cam_get_data(void);

#endif
