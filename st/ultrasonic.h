#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

/* 超声波运行参数 */
#define ULTRASONIC_MAX_TIME_US  66000  /* 超时阈值（us），超过判定为无效 */
#define ULTRASONIC_PERIOD_MS    70     /* 轮询周期（ms），不能小于 60ms */
#define ULTRASONIC_SOUND_SPEED  340.0f /* 声速（m/s） */

/* 超声波状态 */
typedef enum {
    ULTRASONIC_STATE_IDLE = 0,
    ULTRASONIC_STATE_WAIT_TRIGGER_END,
    ULTRASONIC_STATE_WAIT_ECHO,
} ultrasonic_state_t;

/* 测距结果 */
typedef struct {
    float distance_mm;
    uint8_t is_valid;
} ultrasonic_data_t;

/* 超声波配置结构体 */
typedef struct {
    GPIO_TypeDef *trig_port;
    uint16_t trig_pin;
} ultrasonic_cfg_t;

/* 超声波句柄 */
typedef struct {
    TIM_HandleTypeDef *htim;
    GPIO_TypeDef *trig_port;
    uint16_t trig_pin;

    uint32_t start_time;
    uint32_t end_time;
    uint32_t last_trigger_tick;

    uint8_t capture_flag;
    ultrasonic_state_t state;
} ultrasonic_handle_t;

void ultrasonic_init(ultrasonic_handle_t *handle,
                     const ultrasonic_cfg_t *cfg,
                     TIM_HandleTypeDef *htim);
void ultrasonic_task(ultrasonic_handle_t *handle);
void ultrasonic_capture_callback(ultrasonic_handle_t *handle,
                                 TIM_HandleTypeDef *htim);
ultrasonic_data_t ultrasonic_get_data(void);

#endif
