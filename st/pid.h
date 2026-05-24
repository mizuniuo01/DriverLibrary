#ifndef __PID_H
#define __PID_H

#include "main.h"

// PID 结构体定义
typedef struct 
{
    float Kp;           // 比例系数            
    float Ki;           // 积分系数
    float Kd;           // 微分系数

    float target;       // 目标值
    float actual;       // 实际值

    float error;        // 当前误差
    float error_last;   // 上一次误差
    float integral;     // 积分值

    float out;          // PID 输出值
    float out_max;      // 输出最大值
    float out_min;      // 输出最小值
    float integral_max; // 积分最大值
} PID_Struct;

// PID相关操作函数声明
void PID_Init(PID_Struct *pid, float p, float i, float d, float out_max, float integral_max);
float PID_Calc(PID_Struct *pid, float target, float actual);
void PID_Clear(PID_Struct *pid);

#endif