#ifndef __PATTERN_H
#define __PATTERN_H

#include "main.h"

// 循迹图案状态枚举
typedef enum 
{
    PATTERN_STRAIGHT = 0,
    PATTERN_SMALL_LEFT_2,
    PATTERN_SMALL_LEFT_1,
    PATTERN_MEDIUM_LEFT,
    PATTERN_BIG_LEFT,
    PATTERN_SMALL_RIGHT_2,
    PATTERN_SMALL_RIGHT_1,
    PATTERN_MEDIUM_RIGHT,
    PATTERN_BIG_RIGHT,
    PATTERN_UNKNOWN
} Pattern_State;

// 函数声明
Pattern_State Pattern_Lookup(uint8_t sensor_data);

#endif