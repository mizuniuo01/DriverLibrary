#include "pid.h"

/**
 * @brief 将控制参数及限幅重置入系统控制器句柄
 * @param pid 目标修改其内部数据的控制柄
 * @param p 比例系数权重设定
 * @param i 积分系数权重设定
 * @param d 微分系数权重设定
 * @param out_max 执行总输出绝对限值
 * @param integral_max 积分环抗饱和最大值
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
    pid->actual_last = 0.0f;
    pid->integral = 0.0f;

    pid->out = 0.0f;
    pid->out_max = out_max;
    pid->out_min = -out_max;
    pid->integral_max = integral_max;
}

/**
 * @brief 执行单次误差反馈调整算法迭代结果
 * @param pid 作用状态柄
 * @param target 用户理想跟踪位置或速度目标
 * @param actual 系统当前的物理实测反馈
 * @retval float 运算结果，应当施加于底层机构的驱动量
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
    
    // 核心输出：微分项作用在实际值上，避免目标突变导致微分尖峰
    pid->out = pid->Kp * pid->error + pid->Ki * pid->integral + pid->Kd * (pid->actual_last - pid->actual);

    pid->error_last = pid->error;
    pid->actual_last = pid->actual;
    
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
    pid->actual_last = 0.0f;
    pid->integral = 0.0f;
    pid->out = 0.0f;
}