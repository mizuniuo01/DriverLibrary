#ifndef __LED_H
#define __LED_H

#include "main.h"

// LED 控制端口宏定义
#define LED_GPIO_PORT   GPIOA

// LED 引脚宏定义
#define LED1_GPIO_PIN   GPIO_PIN_0
#define LED2_GPIO_PIN   GPIO_PIN_1

// LED 状态控制函数声明
void LED1_ON(void);
void LED1_OFF(void);
void LED1_TOGGLE(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED2_TOGGLE(void);
void LED_ShowSystemStatus(void);

#endif