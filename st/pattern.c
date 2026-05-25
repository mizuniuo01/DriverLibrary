/**
 * @file    pattern.c
 * @brief   循迹图案识别模块（查表法）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    纯算法模块，无硬件依赖，输入 8 位传感器数据
 * @note    bit7=最左侧，bit0=最右侧
 *
 * @usage
 * pattern_state_t state = pattern_lookup(sensor_data);
 */

#include "pattern.h"

/**
 * @brief  查表法匹配 8 位循迹传感器数据
 * @param  sensor_data  8 位传感器数据
 * @retval 当前图案状态
 */
pattern_state_t pattern_lookup(uint8_t sensor_data)
{
    /* 直线（中间传感器触发） */
    if (sensor_data == 0b00011000
        || sensor_data == 0b00001000
        || sensor_data == 0b00010000) {
        return PATTERN_STRAIGHT;
    }

    /* 直线过滤（抗噪与杂散点兼容） */
    if (sensor_data == 0b10011001
        || sensor_data == 0b00011001
        || sensor_data == 0b10011000
        || sensor_data == 0b10001000
        || sensor_data == 0b00001001
        || sensor_data == 0b00010001
        || sensor_data == 0b10001001
        || sensor_data == 0x98
        || sensor_data == 0xD8
        || sensor_data == 0x78
        || sensor_data == 0x38
        || sensor_data == 0x19
        || sensor_data == 0x1B
        || sensor_data == 0x1E
        || sensor_data == 0x1C) {
        return PATTERN_STRAIGHT;
    }

    /* 微偏左 2 级 */
    if (sensor_data == 0b00000110) {
        return PATTERN_SMALL_LEFT_2;
    }

    /* 微偏左 1 级 */
    if (sensor_data == 0b00000100) {
        return PATTERN_SMALL_LEFT_1;
    }

    /* 微偏右 2 级 */
    if (sensor_data == 0b01100000) {
        return PATTERN_SMALL_RIGHT_2;
    }

    /* 微偏右 1 级 */
    if (sensor_data == 0b00100000) {
        return PATTERN_SMALL_RIGHT_1;
    }

    /* 中偏左 */
    if (sensor_data == 0b00000010) {
        return PATTERN_MEDIUM_LEFT;
    }

    /* 中偏右 */
    if (sensor_data == 0b01000000) {
        return PATTERN_MEDIUM_RIGHT;
    }

    /* 大偏左 */
    if (sensor_data == 0b00000001
        || sensor_data == 0b00000011) {
        return PATTERN_BIG_LEFT;
    }

    /* 大偏右 */
    if (sensor_data == 0b10000000
        || sensor_data == 0b11000000) {
        return PATTERN_BIG_RIGHT;
    }

    return PATTERN_UNKNOWN;
}
