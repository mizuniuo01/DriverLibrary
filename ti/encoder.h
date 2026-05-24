#ifndef __ENCODER_H
#define __ENCODER_H

#include "header.h"

// 编码器数据结构体
typedef struct {
    int16_t left_val;  // 纯硬件 QEI 读数
    int16_t right_val; // 捕获中断计算读数
} Encoder_Data_t;

extern volatile Encoder_Data_t encoderData;

// 全局暴露给捕获中断使用的右轮软件计数值
extern volatile uint16_t timer_capture_right_count;

// 编码器初始化与数据获取接口
void Encoder_Init(GPTIMER_Regs *htim_left_qei, GPTIMER_Regs *htim_right_capture);
void Encoder_Get_Left(GPTIMER_Regs *htim_left_qei);
void Encoder_Get_Right(void);

#endif
