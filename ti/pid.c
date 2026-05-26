/**
 * @file    pid.c
 * @brief   PID 控制器模块（微分-on-实际值）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    纯算法模块，无硬件依赖
 * @note    微分项作用在实际值上，避免目标突变导致微分尖峰
 * @note    含积分饱和限幅和输出限幅
 * @note    错误码：init 判空返回 DRV_ERR_PARAM
 *
 * @usage
 * pid_t pid;
 * pid_init(&pid, 1.5f, 0.1f, 0.05f, 500.0f, 200.0f);
 * float out = pid_calc(&pid, target, actual);
 * pid_clear(&pid);
 */

#include "pid.h"

/**
 * @brief  PID 初始化
 * @param  pid           PID 句柄指针
 * @param  p             比例系数
 * @param  i             积分系数
 * @param  d             微分系数
 * @param  out_max       输出限幅
 * @param  integral_max  积分饱和限幅
 * @retval DRV_OK 成功，DRV_ERR_PARAM 参数非法
 */
drv_err_t pid_init(pid_t *pid, float p, float i, float d, float out_max, float integral_max)
{
    if (!pid) {
        return DRV_ERR_PARAM;
    }

    pid->kp = p;
    pid->ki = i;
    pid->kd = d;

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

    return DRV_OK;
}

/**
 * @brief  PID 计算（微分-on-实际值）
 * @param  pid     PID 句柄指针
 * @param  target  目标值
 * @param  actual  实际值
 * @retval PID 输出值
 */
float pid_calc(pid_t *pid, float target, float actual)
{
    if (!pid) {
        return 0.0f;
    }

    pid->target = target;
    pid->actual = actual;

    /* 计算本次误差 */
    pid->error = pid->target - pid->actual;

    /* 积分项累加 */
    pid->integral += pid->error;

    /* 积分饱和限幅 */
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < -pid->integral_max) {
        pid->integral = -pid->integral_max;
    }

    /* 微分-on-实际值，避免目标突变尖峰 */
    pid->out = pid->kp * pid->error + pid->ki * pid->integral +
               pid->kd * (pid->actual_last - pid->actual);

    pid->error_last = pid->error;
    pid->actual_last = pid->actual;

    /* 输出限幅 */
    if (pid->out > pid->out_max) {
        pid->out = pid->out_max;
    } else if (pid->out < pid->out_min) {
        pid->out = pid->out_min;
    }

    return pid->out;
}

/**
 * @brief  清除 PID 历史状态
 * @param  pid  PID 句柄指针
 * @retval 无
 */
void pid_clear(pid_t *pid)
{
    if (!pid) {
        return;
    }

    pid->error = 0.0f;
    pid->error_last = 0.0f;
    pid->actual_last = 0.0f;
    pid->integral = 0.0f;
    pid->out = 0.0f;
}
