/**
 * @file    laser.c
 * @brief   激光驱动模块（GPIO 控制）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：GPIO 已在 SysConfig 中配置
 *
 * @usage
 * static laser_handle_t laser;
 *
 * laser_cfg_t cfg = { .port = LASER_PORT, .pin = LASER_PIN };
 * laser_init(&laser, &cfg);
 * laser_on(&laser);
 * laser_off(&laser);
 * laser_toggle(&laser);
 */

#include "laser.h"

/**
 * @brief  激光初始化
 * @param  handle  激光句柄指针
 * @param  cfg     激光配置指针
 * @retval 无
 */
void laser_init(laser_handle_t *handle, const laser_cfg_t *cfg)
{
    if (!handle || !cfg) {
        return;
    }

    handle->port = cfg->port;
    handle->pin = cfg->pin;

    DL_GPIO_clearPins(handle->port, handle->pin);
}

/**
 * @brief  打开激光
 * @param  handle  激光句柄指针
 * @retval 无
 */
void laser_on(laser_handle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_setPins(handle->port, handle->pin);
}

/**
 * @brief  关闭激光
 * @param  handle  激光句柄指针
 * @retval 无
 */
void laser_off(laser_handle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_clearPins(handle->port, handle->pin);
}

/**
 * @brief  翻转激光状态
 * @param  handle  激光句柄指针
 * @retval 无
 */
void laser_toggle(laser_handle_t *handle)
{
    if (!handle) {
        return;
    }

    DL_GPIO_togglePins(handle->port, handle->pin);
}
