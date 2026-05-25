#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

#define GYRO_DMA_RX_BUF_SIZE 128 /* DMA 单次接收最大缓存量 */
#define GYRO_RX_FIFO_SIZE    512 /* 接收环形队列容量 */
#define GYRO_FRAME_HEADER    0x55 /* 帧头标识 */
#define GYRO_TYPE_ANGLE      0x53 /* 角度数据类型 */
#define GYRO_FRAME_LEN       11 /* 角度数据帧长度（含校验） */
#define GYRO_MAX_DELTA_DEG   50.0f /* 单周期偏航角变化上限（度） */

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
    UART_HandleTypeDef *huart;

    uint8_t dma_rx_buffer[GYRO_DMA_RX_BUF_SIZE];
    uint8_t rx_fifo[GYRO_RX_FIFO_SIZE];
    uint16_t rx_write_pos;
    uint16_t rx_read_pos;

    gyro_frame_state_t rx_state;
    uint8_t frame_buffer[GYRO_FRAME_LEN];
    uint16_t frame_index;
} gyro_handle_t;

/* gyro_task 中 yaw 增量更新标志位（ISR 置1，task 清0） */
extern volatile uint8_t gyro_tick_flag;

void gyro_init(UART_HandleTypeDef *huart);
void gyro_rx_callback(UART_HandleTypeDef *huart, uint16_t size);
void gyro_error_callback(UART_HandleTypeDef *huart);
void gyro_task(void);
void gyro_on_camera_frame(void);
float gyro_get_delta_yaw(void);
gyro_data_t gyro_get_data(void);

#endif
