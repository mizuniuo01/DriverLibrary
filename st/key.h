#ifndef KEY_H
#define KEY_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

#define KEY_MAX_COUNT        8  /* 单句柄最大按键数 */
#define KEY_TASK_PERIOD_MS   10 /* key_task 调度周期(ms) */

/* key_task 调度标志位（ISR 置1，task 清0） */
extern volatile uint8_t key_task_flag;

/* 单个按键引脚配置 */
typedef struct {
    uint16_t pin;
    uint8_t id;
} key_pin_cfg_t;

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_REPEAT,
} key_event_t;

/* 按键句柄 */
typedef struct key_handle_t key_handle_t;

/* 按键事件回调函数类型 */
typedef void (*key_callback_t)(key_handle_t *handle, uint8_t key_id,
                               key_event_t event);

struct key_handle_t {
    GPIO_TypeDef *port;
    uint8_t key_count;
    key_pin_cfg_t pin_cfgs[KEY_MAX_COUNT];

    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t repeat_ms;

    /* ISR 写入，task 读取，需 volatile */
    volatile uint16_t press_cnt[KEY_MAX_COUNT];
    volatile uint8_t state[KEY_MAX_COUNT];
    volatile uint8_t just_pressed[KEY_MAX_COUNT];
    volatile uint8_t just_released[KEY_MAX_COUNT];

    /* task 写入，getter 读取 */
    volatile key_event_t event[KEY_MAX_COUNT];

    /* task 私有，ISR 不访问 */
    uint16_t hold_cnt[KEY_MAX_COUNT];

    key_callback_t callback;
};

void key_init(key_handle_t *handle, GPIO_TypeDef *port,
              const key_pin_cfg_t *pin_cfgs,
              uint8_t key_count, uint16_t debounce_ms,
              uint16_t long_press_ms, uint16_t repeat_ms);
void key_set_callback(key_handle_t *handle, key_callback_t callback);
void key_scan_task(key_handle_t *handle);
void key_task(key_handle_t *handle);
key_event_t key_get_event(key_handle_t *handle, uint8_t key_id);
uint8_t key_is_pressed(key_handle_t *handle, uint8_t key_id);

#endif
