#ifndef __PWM_H
#define __PWM_H

#include "main.h"

#define PWM_MAX_COMPARE 8400

// PWM启动与设置接口
void PWM_Init(TIM_HandleTypeDef *htim);
void PWM_SetCompare3(TIM_HandleTypeDef *htim, uint16_t compare);
void PWM_SetCompare4(TIM_HandleTypeDef *htim, uint16_t compare);

#endif