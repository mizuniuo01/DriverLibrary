#ifndef PID_H
#define PID_H

#include <stdint.h>
#include "drv_err.h"

/* PID 控制器 */
typedef struct {
    float kp; /* 比例系数 */
    float ki; /* 积分系数 */
    float kd; /* 微分系数 */

    float target; /* 目标值 */
    float actual; /* 实际值 */

    float error;      /* 当前误差 */
    float error_last; /* 上一次误差 */
    float integral;   /* 积分累加 */

    float out;          /* PID 输出 */
    float out_max;      /* 输出上限 */
    float out_min;      /* 输出下限 */
    float integral_max; /* 积分限幅 */
} pid_t;

drv_err_t pid_init(pid_t *pid, float p, float i, float d, float out_max, float integral_max);
float pid_calc(pid_t *pid, float target, float actual);
void pid_clear(pid_t *pid);

#endif
