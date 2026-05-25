/**
 * @file    buzzer.c
 * @brief   蜂鸣器驱动模块（GPIO 控制）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：GPIO 已在 SysConfig 中配置
 *
 * @usage
 * 驱动层模块，负责单一 GPIO 的开关控制，不管理时序。多实例设计，
 * 引脚通过 buzzer_init() 注入，不依赖具体硬件宏。
 *
 * 基本用法：
 *
 * static BuzzerHandle_t buzzer;
 *
 * void system_init(void)
 * {
 *     BuzzerCfg_t cfg = { .port = BUZZER_PORT,
 *                          .pin  = BUZZER_BUZZER1_PIN };
 *     buzzer_init(&buzzer, &cfg);
 * }
 *
 * buzzer_on(&buzzer);
 * buzzer_off(&buzzer);
 * buzzer_toggle(&buzzer);
 *
 * 非阻塞鸣叫模式请在应用层用状态机 + tick 实现，
 * buzzer 模块不提供定时自动关闭功能。
 */

#include "buzzer.h"

/**
 * @brief  蜂鸣器初始化
 * @param  handle  蜂鸣器句柄指针
 * @param  cfg     蜂鸣器配置指针
 * @retval 无
 */
void buzzer_init(BuzzerHandle_t *handle, const BuzzerCfg_t *cfg)
{
    if (!handle || !cfg) {
        return;
    }

    handle->port = cfg->port;
    handle->pin = cfg->pin;

    /* 初始状态：关闭 */
    DL_GPIO_clearPins(handle->port, handle->pin);
}

/**
 * @brief  打开蜂鸣器
 * @param  handle  蜂鸣器句柄指针
 * @retval 无
 */
void buzzer_on(BuzzerHandle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_setPins(handle->port, handle->pin);
}

/**
 * @brief  关闭蜂鸣器
 * @param  handle  蜂鸣器句柄指针
 * @retval 无
 */
void buzzer_off(BuzzerHandle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_clearPins(handle->port, handle->pin);
}

/**
 * @brief  翻转蜂鸣器状态
 * @param  handle  蜂鸣器句柄指针
 * @retval 无
 */
void buzzer_toggle(BuzzerHandle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_togglePins(handle->port, handle->pin);
}
