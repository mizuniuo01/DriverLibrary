#ifndef __PWM_H
#define __PWM_H

#include "header.h"

// 依据 Sysconfig 中配置的 pwmMode / ccValue 设定的周期界定
#define PWM_MAX_COMPARE 2000

// 函数声明
void PWM_Init(GPTIMER_Regs *htim);
void PWM_Set_Compare_CH0(GPTIMER_Regs *htim, uint16_t compare);
void PWM_Set_Compare_CH1(GPTIMER_Regs *htim, uint16_t compare);

#endif