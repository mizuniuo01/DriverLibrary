#ifndef CAM_H
#define CAM_H

#include <stdint.h>
#include <stm32f4xx_hal.h>

#define CAM_DMA_RX_BUF_SIZE 128 /* DMA 单次接收最大缓存量 */
#define CAM_RX_FIFO_SIZE 512    /* 接收环形队列容量 */
#define CAM_MAX_FRAME_LEN 128   /* 单帧最大长度 */

#define CAM_FRAME_HEADER 0xFF /* 帧头标识 */
#define CAM_FRAME_TAIL 0xFE   /* 帧尾标识 */

typedef enum {
    CAM_STATE_WAIT_HEADER = 0,
    CAM_STATE_RECEIVING_DATA,
} cam_frame_state_t;

typedef struct {
    UART_HandleTypeDef *huart;

    uint8_t dma_rx_buffer[CAM_DMA_RX_BUF_SIZE];
    uint8_t rx_fifo[CAM_RX_FIFO_SIZE];
    uint16_t rx_write_pos;
    uint16_t rx_read_pos;

    cam_frame_state_t rx_state;
    uint8_t frame_buffer[CAM_MAX_FRAME_LEN];
    uint16_t frame_index;
} cam_handle_t;

/* 摄像头解析数据（根据实际数据格式定义） */
typedef struct {
    uint8_t reserved[32];
} cam_data_t;

void cam_init(UART_HandleTypeDef *huart);
void cam_rx_callback(UART_HandleTypeDef *huart, uint16_t size);
void cam_task(void);
cam_data_t cam_get_data(void);

#endif
