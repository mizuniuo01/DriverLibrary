#ifndef __PATTERN_H
#define __PATTERN_H

#include <stdint.h>

// 循迹图案状态枚举
typedef enum 
{
    PATTERN_STRAIGHT = 0,
    PATTERN_SMALL_LEFT,
    PATTERN_SMALL_RIGHT,
    PATTERN_MEDIUM_LEFT,
    PATTERN_MEDIUM_RIGHT,
    PATTERN_BIG_LEFT,
    PATTERN_BIG_RIGHT,
    PATTERN_RIGHT_ANGLE_LEFT,
    PATTERN_RIGHT_ANGLE_RIGHT,
    PATTERN_CROSS,
    PATTERN_UNKNOWN,
    PATTERN_COUNT
} Pattern_State;

// 函数声明
Pattern_State Pattern_Lookup(uint8_t sensor_data);

#endif