#include "pid.h"

/**
 * @brief PID参数初始化
 * @param pid PID结构体句柄
 * @param p 比例系数
 * @param i 积分系数
 * @param d 微分系数
 * @param out_max 输出限幅
 * @param integral_max 积分限幅
 * @retval None
 */
void PID_Init(PID_Struct *pid, float p, float i, float d, float out_max, float integral_max)
{
    if (pid == NULL)
    {
        return;
    }

    pid->Kp = p;
    pid->Ki = i;
    pid->Kd = d;
    
    pid->target = 0.0f;
    pid->actual = 0.0f;
    
    pid->error = 0.0f;
    pid->error_last = 0.0f;
    pid->integral = 0.0f;
    
    pid->out = 0.0f;
    pid->out_max = out_max;
    pid->out_min = -out_max;
    pid->integral_max = integral_max;
}

/**
 * @brief PID计算执行
 * @param pid PID结构体句柄
 * @param target 目标值
 * @param actual 实际值
 * @retval float 计算出的PID控制量
 */
float PID_Calc(PID_Struct *pid, float target, float actual)
{
    if (pid == NULL)
    {
        return 0.0f;
    }

    pid->target = target;
    pid->actual = actual;
    
    // 计算本次误差
    pid->error = pid->target - pid->actual;
    
    // 积分项累加
    pid->integral += pid->error;
    
    // 限制积分上限，防止积分饱和导致控制超调
    if (pid->integral > pid->integral_max) 
    {
        pid->integral = pid->integral_max;
    } 
    else if (pid->integral < -pid->integral_max) 
    {
        pid->integral = -pid->integral_max;
    }
    
    // 核心输出：离散式位置PID公式实现
    pid->out = pid->Kp * pid->error + pid->Ki * pid->integral + pid->Kd * (pid->error - pid->error_last);
               
    pid->error_last = pid->error;
    
    // 总输出限幅确保底层驱动板安全
    if (pid->out > pid->out_max) 
    {
        pid->out = pid->out_max;
    } 
    else if (pid->out < pid->out_min) 
    {
        pid->out = pid->out_min;
    }
    
    return pid->out;
}

/**
 * @brief 清空PID状态(复位)
 * @param pid PID结构体句柄
 * @retval None
 */
void PID_Clear(PID_Struct *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->error = 0.0f;
    pid->error_last = 0.0f;
    pid->integral = 0.0f;
    pid->out = 0.0f;
}