#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

#define GYRO_DMA_RX_BUF_SIZE 128 /* DMA 单次接收最大缓存量 */
#define GYRO_RX_FIFO_SIZE 512    /* 接收环形队列容量 */
#define GYRO_FRAME_HEADER 0x55   /* 帧头标识 */
#define GYRO_TYPE_ANGLE 0x53     /* 角度数据类型 */
#define GYRO_FRAME_LEN 11        /* 角度数据帧长度（含校验） */

typedef enum {
    GYRO_STATE_WAIT_HEADER = 0,
    GYRO_STATE_WAIT_TYPE,
    GYRO_STATE_RECEIVING_DATA,
} gyro_frame_state_t;

typedef struct {
    float roll;
    float pitch;
    float yaw;
} gyro_data_t;

typedef struct {
    UART_Regs *huart;

    uint8_t dma_rx_buffer[GYRO_DMA_RX_BUF_SIZE];
    uint8_t rx_fifo[GYRO_RX_FIFO_SIZE];
    volatile uint16_t rx_write_pos;
    volatile uint16_t rx_read_pos;

    gyro_frame_state_t rx_state;
    uint8_t frame_buffer[GYRO_FRAME_LEN];
    uint16_t frame_index;
} gyro_handle_t;

/* 软件 IDLE 检测标志位（定时器 ISR 中周期性置 1） */
extern volatile uint8_t gyro_check_idle_flag;

void gyro_init(UART_Regs *huart);
void gyro_rx_callback(UART_Regs *huart, uint16_t size);
void gyro_error_callback(UART_Regs *huart);
void gyro_task(void);
gyro_data_t gyro_get_data(void);

#endif
