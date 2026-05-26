/**
 * @file    sensor.c
 * @brief   通用无 MCU 灰度传感器驱动（GPIO 直接读取）
 * @author  mizuniuo01
 * @date    2026-05-26
 * @version 1.0.0
 * @note    适配任何无 MCU 的灰度传感器模块，通道数和有效电平通过 cfg 配置
 * @note    sensor_task 由 sensor_tick_flag 触发，非阻塞
 * @note    错误码：init 判空返回 DRV_ERR_PARAM
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 每路传感器对应一个 GPIO 输入引脚，直接读取电平。
 *
 * ── 配置 ──
 *
 * sensor_cfg_t cfg = {
 *     .channels = {
 *         {PORTA, DL_GPIO_PIN_0},  // 最左侧探头
 *         {PORTA, DL_GPIO_PIN_1},
 *         {PORTA, DL_GPIO_PIN_2},
 *         {PORTA, DL_GPIO_PIN_3},
 *         {PORTB, DL_GPIO_PIN_0},
 *         {PORTB, DL_GPIO_PIN_1},
 *         {PORTB, DL_GPIO_PIN_2},  // 最右侧探头
 *     },
 *     .channel_count = 7,
 *     .active_level = 0,  // 低电平(0)=黑线，模块内部取反
 * };
 *
 * ── 初始化 ──
 *
 * sensor_init(&cfg);
 *
 * ── ISR 中置 sensor_tick_flag ──
 *
 * ── 主循环 ──
 *
 * sensor_task();  // 检查 tick_flag → 读所有 GPIO → 组装位掩码
 *
 * ── 数据读取 ──
 *
 * uint8_t data = sensor_read_data();
 * // bit0=最左侧探头 ... bit(N-1)=最右侧探头，1=黑线
 * // 可直接传给 pattern_lookup(data)
 */

#include "sensor.h"

static sensor_cfg_t sensor_cfg;
static uint8_t sensor_data;

volatile uint8_t sensor_tick_flag;

/**
 * @brief  传感器初始化
 * @param  cfg  传感器配置指针（通道引脚、数量、有效电平）
 * @retval DRV_OK 成功，DRV_ERR_PARAM 参数非法
 */
drv_err_t sensor_init(const sensor_cfg_t *cfg)
{
    uint8_t i;

    if (!cfg || cfg->channel_count == 0
        || cfg->channel_count > SENSOR_MAX_CHANNELS) {
        return DRV_ERR_PARAM;
    }

    sensor_cfg.channel_count = cfg->channel_count;
    sensor_cfg.active_level = cfg->active_level;

    for (i = 0; i < cfg->channel_count; i++) {
        sensor_cfg.channels[i].port = cfg->channels[i].port;
        sensor_cfg.channels[i].pin = cfg->channels[i].pin;
    }

    sensor_data = 0;

    return DRV_OK;
}

/**
 * @brief  传感器轮询任务（主循环中调用）
 * @note   由 sensor_tick_flag 触发，读所有 GPIO 通道并组装位掩码
 * @note   DL_GPIO_readPins 读指定引脚的当前电平，返回 0 或 pin 掩码
 * @param  无
 * @retval 无
 */
void sensor_task(void)
{
    uint8_t i;
    uint8_t result;

    if (!sensor_tick_flag) {
        return;
    }
    sensor_tick_flag = 0;

    result = 0;

    for (i = 0; i < sensor_cfg.channel_count; i++) {
        uint32_t port_val;
        uint8_t bit_val;

        port_val = DL_GPIO_readPins(
            sensor_cfg.channels[i].port, sensor_cfg.channels[i].pin);

        if (sensor_cfg.active_level == 0) {
            /* 低电平=黑线：读到 0 → bit 置 1 */
            bit_val = (port_val == 0) ? 1 : 0;
        } else {
            /* 高电平=黑线：读到非 0 → bit 置 1 */
            bit_val = (port_val != 0) ? 1 : 0;
        }

        result |= (bit_val << i);
    }

    sensor_data = result;
}

/**
 * @brief  读取转换后的传感器数据
 * @param  无
 * @retval 位掩码（bit0=最左探头, 1=黑线）
 */
uint8_t sensor_read_data(void)
{
    return sensor_data;
}
