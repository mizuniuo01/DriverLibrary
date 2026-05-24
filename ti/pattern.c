#include "pattern.h"

/**
 * @brief 查表法匹配寻迹传感器传回的 8 位数据状态
 * @param sensor_data 8位传感器整合数据 (bit7为最左侧，bit0为最右侧)
 * @retval Pattern_State 当前的图案状态
 */
Pattern_State Pattern_Lookup(uint8_t sensor_data)
{
    // 直线状态 (中间传感器触发：bit3, bit4)
    if (sensor_data == 0b00011000 || sensor_data == 0b00001000 || sensor_data == 0b00010000) return PATTERN_STRAIGHT;

    // 直线过滤 (抗噪与杂散点兼容)
    if (sensor_data == 0b10011001 || sensor_data == 0b00011001 || sensor_data == 0b10011000 ||
        sensor_data == 0b10001000 || sensor_data == 0b00001001 || sensor_data == 0b00010001 ||
        sensor_data == 0b10001001 || sensor_data == 0x98     || sensor_data == 0xD8     || 
        sensor_data == 0x78     || sensor_data == 0x38     || sensor_data == 0x19     || 
        sensor_data == 0x1B     || sensor_data == 0x1E     || sensor_data == 0x1C) return PATTERN_STRAIGHT;

    // 微偏左 (1出现在视野偏左，即 bit1, bit2) -> 二进制右侧
    if (sensor_data == 0b00000110 || sensor_data == 0b00000100) return PATTERN_SMALL_LEFT;

    // 微偏右 (1出现在视野偏右，即 bit5, bit6) -> 二进制左侧
    if (sensor_data == 0b01100000 || sensor_data == 0b00100000) return PATTERN_SMALL_RIGHT;

    // 中偏左 (bit1左右)
    if (sensor_data == 0b00000010 || sensor_data == 0b00000011) return PATTERN_MEDIUM_LEFT;

    // 中偏右 (bit6左右)
    if (sensor_data == 0b01000000 || sensor_data == 0b11000000) return PATTERN_MEDIUM_RIGHT;

    // 左大偏 (1出现在最左侧，即 bit0) -> 二进制最右边
    if (sensor_data == 0b00000001) return PATTERN_BIG_LEFT;

    // 右大偏 (1出现在最右侧，即 bit7) -> 二进制最左边
    if (sensor_data == 0b10000000) return PATTERN_BIG_RIGHT;

    // 直角弯 (单侧全1，另一侧全0)
    if (sensor_data == 0b00001111 || sensor_data == 0b00011111 || sensor_data == 0b00111111 || sensor_data == 0b00000111) return PATTERN_RIGHT_ANGLE_LEFT;
    if (sensor_data == 0b11110000 || sensor_data == 0b11111000 || sensor_data == 0b11111100 || sensor_data == 0b11100000) return PATTERN_RIGHT_ANGLE_RIGHT;

    // 起始、终点线或十字路口 (全1)
    if (sensor_data == 0b11111111 || sensor_data == 0b01111111 || sensor_data == 0b11111110) return PATTERN_CROSS;
    return PATTERN_UNKNOWN;
}