#ifndef PATTERN_H
#define PATTERN_H

#include <stdint.h>

/* 循迹图案状态 */
typedef enum {
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
} pattern_state_t;

pattern_state_t pattern_lookup(uint8_t sensor_data);

#endif
