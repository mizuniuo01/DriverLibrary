#ifndef __MOTOR_H
#define __MOTOR_H

#include "header.h"

// 速度限定（PWM比较值，量程0~PWM_MAX_COMPARE=2000）
#define MOTOR_MAX_SPEED 2000
#define MOTOR_MIN_SPEED 0
// 电机有效最小驱动值（低于该值按0输出）
#define MOTOR_EFFECTIVE_MIN_SPEED 60

void Motor_Init(void);
void Motor_Set_Speed_1(GPTIMER_Regs *htim, int16_t speed);
void Motor_Set_Speed_2(GPTIMER_Regs *htim, int16_t speed);
extern volatile int8_t motor_right_direction_sign;

#endif
