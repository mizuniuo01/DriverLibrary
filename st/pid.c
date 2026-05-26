/**
 * @file    pid.c
 * @brief   PID 控制器模块（微分-on-误差）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    纯算法模块，无硬件依赖
 * @note    微分项作用在误差上（标准位置式 PID）
 * @note    含积分饱和限幅和输出限幅
 *
 * @usage
 * pid_t pid;
 * pid_init(&pid, 1.5f, 0.1f, 0.05f, 500.0f, 200.0f);
 * float out = pid_calc(&pid, target, actual);
 * pid_clear(&pid);
 */

#include "pid.h"

void pid_init(pid_t *pid, float p, float i, float d, float out_max, float integral_max)
{
    if (!pid) {
        return;
    }

    pid->kp = p;
    pid->ki = i;
    pid->kd = d;

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

    /* 标准位置式 PID */
    pid->out = pid->kp * pid->error + pid->ki * pid->integral +
               pid->kd * (pid->error - pid->error_last);

    pid->error_last = pid->error;

    /* 输出限幅 */
    if (pid->out > pid->out_max) {
        pid->out = pid->out_max;
    } else if (pid->out < pid->out_min) {
        pid->out = pid->out_min;
    }

    return pid->out;
}

void pid_clear(pid_t *pid)
{
    if (!pid) {
        return;
    }

    pid->error = 0.0f;
    pid->error_last = 0.0f;
    pid->integral = 0.0f;
    pid->out = 0.0f;
}
