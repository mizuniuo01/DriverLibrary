#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stm32f4xx_hal.h>
#include "ring_buffer.h"
#include "fashion_star_uart_servo.h"

/* 缓冲区大小 */
typedef enum {
    SERVO_DMA_RX_BUF_SIZE = 128, /* DMA 单次接收缓冲区 */
    SERVO_RX_FIFO_SIZE = 256,    /* 接收环形队列容量 */
    SERVO_TX_FIFO_SIZE = 256,    /* 发送环形队列容量 */
} servo_buf_size_t;

/* 默认到达时间（ms） */
#define SERVO_DEFAULT_INTERVAL_MS 100

/* 舵机配置（句柄注入） */
typedef struct {
    UART_HandleTypeDef *huart;   /* 串口句柄 */
    uint8_t servo_id_x;          /* X 轴舵机 ID */
    uint8_t servo_id_y;          /* Y 轴舵机 ID */
    float x_angle_min;           /* X 轴角度下限（度） */
    float x_angle_max;           /* X 轴角度上限（度） */
    float y_angle_min;           /* Y 轴角度下限（度） */
    float y_angle_max;           /* Y 轴角度上限（度） */
    float center_angle_x;        /* X 轴中心角度（度） */
    float center_angle_y;        /* Y 轴中心角度（度） */
    uint16_t default_interval_ms; /* 默认到达时间（ms） */
} servo_cfg_t;

/* 舵机句柄 */
typedef struct {
    Usart_DataTypeDef usart;                       /* 官方库通信句柄 */
    servo_cfg_t cfg;                               /* 配置副本 */
    uint8_t dma_rx_buf[SERVO_DMA_RX_BUF_SIZE];     /* DMA 接收缓冲 */
    uint8_t rx_fifo_buf[SERVO_RX_FIFO_SIZE];       /* 接收 FIFO 内存 */
    uint8_t tx_fifo_buf[SERVO_TX_FIFO_SIZE];       /* 发送 FIFO 内存 */
    RingBufferTypeDef rx_ring;                     /* 接收环形队列 */
    RingBufferTypeDef tx_ring;                     /* 发送环形队列 */
} servo_handle_t;

void servo_init(servo_handle_t *handle, const servo_cfg_t *cfg);
void servo_rx_callback(servo_handle_t *handle,
                       UART_HandleTypeDef *huart,
                       uint16_t size);
void servo_tx_task(servo_handle_t *handle);
void servo_set_angle(servo_handle_t *handle,
                     uint8_t servo_id,
                     float angle,
                     uint16_t interval_ms);
void servo_set_sync_angle(servo_handle_t *handle,
                          float angle_x,
                          float angle_y,
                          uint16_t interval_ms);
void servo_get_angle(servo_handle_t *handle,
                     uint8_t servo_id,
                     float *angle);
void servo_reset_center(servo_handle_t *handle);
void servo_release(servo_handle_t *handle, uint8_t servo_id);
void servo_ping(servo_handle_t *handle, uint8_t servo_id);

#endif
