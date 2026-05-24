#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"

// 编码器数据结构体
typedef struct {
    int16_t left_val;
    int16_t right_val;
} Encoder_Data_t;

extern Encoder_Data_t encoderData;

// 编码器初始化与数据获取接口
void Encoder_Init(TIM_HandleTypeDef *htim_left, TIM_HandleTypeDef *htim_right);
void Encoder_Get_Left(TIM_HandleTypeDef *htim);
void Encoder_Get_Right(TIM_HandleTypeDef *htim);

#endif