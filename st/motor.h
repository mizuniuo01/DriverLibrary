#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h"
#include "pwm.h"

// 电机限幅常量宏定义
#define MOTOR_MAX_SPEED 100
#define MOTOR_MIN_SPEED 0

void Motor_Init(TIM_HandleTypeDef *htim);
// 电机设置接口
void Motor_SetSpeed1(TIM_HandleTypeDef *htim, int16_t speed);
void Motor_SetSpeed2(TIM_HandleTypeDef *htim, int16_t speed);

#endif