/**
 * @file    key.c
 * @brief   按键驱动模块（消抖 + 短按/长按/连发检测）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 2.0.0
 * @note    依赖：GPIO 已在 CubeMX 中配置（输入模式，内部上拉）
 * @note    key_scan_task 为 B 类操作，在 ISR 1ms tick 中直接调用
 * @note    key_task 为 A 类操作，由 key_task_flag 触发，调度周期 KEY_TASK_PERIOD_MS
 * @warning key_scan_task 中禁止浮点运算、循环等待和 HAL_Delay
 * @note    参数非法时通过 error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM) 上报
 *
 * @usage
 * 按键模块采用两层 task 架构：
 * - key_scan_task（B 类，ISR 1ms）：积分消抖 + 边沿检测
 * - key_task     （A 类，主循环 10ms）：长按/连发时序判断
 *
 * 完整用法：
 *
 * static key_handle_t keys;
 *
 * const key_pin_cfg_t pins[] = {
 *     {KEY1_Pin, 0},
 *     {KEY2_Pin, 1},
 *     {KEY3_Pin, 2},
 *     {KEY4_Pin, 3},
 * };
 *
 * void system_init(void)
 * {
 *     key_init(&keys, KEY_GPIO_Port, pins, 4,
 *              30,    // 消抖 30ms
 *              800,   // 长按阈值 800ms
 *              100);  // 连发间隔 100ms（0=禁用连发）
 * }
 *
 * // ISR 中（1ms）：B 类直接调用
 * void SysTick_Handler(void)
 * {
 *     key_scan_task(&keys);
 *     // ... 10ms 分频后置 key_task_flag = 1
 * }
 *
 * // 主循环中：A 类 flag 触发
 * void main(void)
 * {
 *     while (1) {
 *         key_task(&keys);   // 内部检查 key_task_flag
 *
 *         key_event_t e = key_get_event(&keys, 0);
 *         if (e == KEY_EVENT_SHORT_PRESS) { ... }
 *         if (e == KEY_EVENT_LONG_PRESS)  { ... }
 *     }
 * }
 *
 * 事件说明：
 * - 短按释放 → KEY_EVENT_SHORT_PRESS（不另发 RELEASE）
 * - 达到长按阈值 → KEY_EVENT_LONG_PRESS（仅一次）
 * - 长按保持 → KEY_EVENT_REPEAT（每 repeat_ms 一次）
 * - 长按后释放 → KEY_EVENT_RELEASE
 * - 回调可选：key_set_callback(&keys, my_handler);
 *
 * 注意：key_task_flag 是模块级全局变量，多个 key 实例共享。
 * 单项目通常只有一组按键，多实例场景需自行管理调度。
 *
 * STM32 与 TI 版差异：
 * - STM32 使用 HAL_GPIO_ReadPin 逐引脚读取，TI 用 DL_GPIO_readPins 批量读取
 * - STM32 引脚类型 uint16_t，TI 为 uint32_t
 */

#include "key.h"
#include "../error_handler.h"

/* 按键消抖状态 */
#define KEY_STATE_IDLE 0    /* 空闲 */
#define KEY_STATE_PRESSED 1 /* 已按下（消抖通过） */

volatile uint8_t key_task_flag;

/**
 * @brief  按键模块初始化
 * @param  handle         按键句柄指针
 * @param  port           按键所在 GPIO 端口
 * @param  pin_cfgs       按键引脚配置数组
 * @param  key_count      按键数量（不超过 KEY_MAX_COUNT）
 * @param  debounce_ms    消抖时间（ms，典型值 20~50）
 * @param  long_press_ms  长按判定时间（ms，典型值 500~1000）
 * @param  repeat_ms      连发间隔（ms，0 表示禁用连发）
 * @retval 无
 */
void key_init(key_handle_t *handle, GPIO_TypeDef *port, const key_pin_cfg_t *pin_cfgs,
    uint8_t key_count, uint16_t debounce_ms, uint16_t long_press_ms, uint16_t repeat_ms)
{
    uint8_t i;

    if (!handle || !port || !pin_cfgs || key_count == 0 || key_count > KEY_MAX_COUNT) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return;
    }

    handle->port = port;
    handle->key_count = key_count;
    handle->debounce_ms = debounce_ms;
    handle->long_press_ms = long_press_ms;
    handle->repeat_ms = repeat_ms;
    handle->callback = NULL;
    key_task_flag = 0;

    for (i = 0; i < key_count; i++) {
        handle->pin_cfgs[i] = pin_cfgs[i];
        handle->press_cnt[i] = 0;
        handle->state[i] = KEY_STATE_IDLE;
        handle->just_pressed[i] = 0;
        handle->just_released[i] = 0;
        handle->event[i] = KEY_EVENT_NONE;
        handle->hold_cnt[i] = 0;
    }
}

/**
 * @brief  设置按键事件回调
 * @param  handle    按键句柄指针
 * @param  callback  回调函数指针（NULL 表示禁用回调）
 * @retval 无
 */
void key_set_callback(key_handle_t *handle, key_callback_t callback)
{
    if (!handle) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return;
    }

    handle->callback = callback;
}

/**
 * @brief  按键扫描任务（B 类，ISR 1ms tick 中直接调用）
 * @note   积分式消抖：按下时计数递增，释放时计数递减
 * @note   只做 GPIO 读取 + 消抖 + 边沿检测，不做长按计时
 * @note   STM32 逐引脚读取 GPIO_PinState，与 TI 批量读取方式不同
 * @param  handle  按键句柄指针
 * @retval 无
 */
void key_scan_task(key_handle_t *handle)
{
    GPIO_PinState pin_state;
    uint16_t pin;
    uint8_t i;

    if (!handle) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return;
    }

    for (i = 0; i < handle->key_count; i++) {
        pin = handle->pin_cfgs[i].pin;
        pin_state = HAL_GPIO_ReadPin(handle->port, pin);

        /* 低电平有效：GPIO_PIN_RESET 表示按下 */
        if (pin_state == GPIO_PIN_RESET) {
            if (handle->press_cnt[i] < handle->debounce_ms) {
                handle->press_cnt[i]++;
            }
        } else {
            if (handle->press_cnt[i] > 0) {
                handle->press_cnt[i]--;
            }
        }

        /* 边沿检测 */
        if (handle->press_cnt[i] >= handle->debounce_ms) {
            if (handle->state[i] == KEY_STATE_IDLE) {
                handle->state[i] = KEY_STATE_PRESSED;
                handle->just_pressed[i] = 1;
            }
        } else if (handle->press_cnt[i] == 0) {
            if (handle->state[i] == KEY_STATE_PRESSED) {
                handle->state[i] = KEY_STATE_IDLE;
                handle->just_released[i] = 1;
            }
        }
    }
}

/**
 * @brief  按键业务任务（A 类，主循环中调用）
 * @note   由 key_task_flag 触发，调度周期 KEY_TASK_PERIOD_MS
 * @note   处理短按/长按/连发判定，生成最终事件
 * @param  handle  按键句柄指针
 * @retval 无
 */
void key_task(key_handle_t *handle)
{
    uint16_t repeat_phase;
    uint8_t i;

    if (!handle) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return;
    }

    if (!key_task_flag) {
        return;
    }
    key_task_flag = 0;

    for (i = 0; i < handle->key_count; i++) {
        /* 按下事件 */
        if (handle->just_pressed[i]) {
            handle->just_pressed[i] = 0;
            handle->hold_cnt[i] = 0;
            handle->event[i] = KEY_EVENT_PRESS;

            if (handle->callback) {
                handle->callback(handle, handle->pin_cfgs[i].id, KEY_EVENT_PRESS);
            }
        } else if (handle->just_released[i]) {
            handle->just_released[i] = 0;

            /* 未达长按阈值 → 短按（短按即释放，不另发 RELEASE） */
            if (handle->hold_cnt[i] < handle->long_press_ms) {
                handle->event[i] = KEY_EVENT_SHORT_PRESS;

                if (handle->callback) {
                    handle->callback(handle, handle->pin_cfgs[i].id,
                        KEY_EVENT_SHORT_PRESS);
                }
            } else {
                /* 长按后释放 → 发 RELEASE */
                handle->event[i] = KEY_EVENT_RELEASE;

                if (handle->callback) {
                    handle->callback(handle, handle->pin_cfgs[i].id, KEY_EVENT_RELEASE);
                }
            }
        } else if (handle->state[i] == KEY_STATE_PRESSED) {
            handle->hold_cnt[i] += KEY_TASK_PERIOD_MS;

            /* 长按判定（>= 防止 task 周期不整除时跳过） */
            if (handle->hold_cnt[i] >= handle->long_press_ms &&
                handle->hold_cnt[i] - KEY_TASK_PERIOD_MS < handle->long_press_ms) {
                handle->event[i] = KEY_EVENT_LONG_PRESS;

                if (handle->callback) {
                    handle->callback(handle, handle->pin_cfgs[i].id,
                        KEY_EVENT_LONG_PRESS);
                }
            }

            /* 连发判定（首次连发在长按后间隔 repeat_ms 才触发） */
            if (handle->repeat_ms > 0 && handle->hold_cnt[i] > handle->long_press_ms) {
                repeat_phase = handle->hold_cnt[i] - handle->long_press_ms;
                if ((repeat_phase % handle->repeat_ms) == 0) {
                    handle->event[i] = KEY_EVENT_REPEAT;

                    if (handle->callback) {
                        handle->callback(handle, handle->pin_cfgs[i].id,
                            KEY_EVENT_REPEAT);
                    }
                }
            }
        }
    }
}

/**
 * @brief  读取并清除指定按键的最新事件
 * @param  handle  按键句柄指针
 * @param  key_id  按键编号
 * @retval 按键事件类型
 */
key_event_t key_get_event(key_handle_t *handle, uint8_t key_id)
{
    uint32_t primask;
    key_event_t evt;
    uint8_t i;

    if (!handle || key_id >= handle->key_count) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return KEY_EVENT_NONE;
    }

    for (i = 0; i < handle->key_count; i++) {
        if (handle->pin_cfgs[i].id == key_id) {
            primask = __get_PRIMASK();
            __disable_irq();

            evt = handle->event[i];
            handle->event[i] = KEY_EVENT_NONE;

            __set_PRIMASK(primask);
            return evt;
        }
    }

    return KEY_EVENT_NONE;
}

/**
 * @brief  查询按键是否处于按下状态
 * @param  handle  按键句柄指针
 * @param  key_id  按键编号
 * @retval 0=未按下，1=按下中
 */
uint8_t key_is_pressed(key_handle_t *handle, uint8_t key_id)
{
    uint8_t i;

    if (!handle || key_id >= handle->key_count) {
        error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
        return 0;
    }

    for (i = 0; i < handle->key_count; i++) {
        if (handle->pin_cfgs[i].id == key_id) {
            error_report(ERROR_SOURCE_KEY, DRV_ERR_PARAM);
            return (handle->state[i] == KEY_STATE_PRESSED) ? 1 : 0;
        }
    }

    return 0;
}
