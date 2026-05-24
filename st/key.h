#ifndef __KEY_H
#define __KEY_H

#include "main.h"

// GPIO 控制端口宏定义
#define KEY_GPIO_PORT          GPIOC

// GPIO 引脚宏定义
#define KEY1_GPIO_PIN          GPIO_PIN_0
#define KEY2_GPIO_PIN          GPIO_PIN_1
#define KEY3_GPIO_PIN          GPIO_PIN_2
#define KEY4_GPIO_PIN          GPIO_PIN_3

// 状态与参数宏定义
#define KEY_PRESSED_STATE      GPIO_PIN_RESET
#define KEY_DEBOUNCE_DELAY_MS  20

// 函数声明
uint8_t Key_GetNum(void);

#endif