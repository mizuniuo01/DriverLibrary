#ifndef PWM_H
#define PWM_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/*
 * PWM 参数均以 20kHz 载波频率为基准。
 * PWM_MAX_COMPARE = 定时器时钟频率 / PWM_FREQ_HZ
 * 例：SysConfig 中定时器时钟 40MHz → CC = 40000000 / 20000 = 2000
 * 若在 SysConfig 中修改了定时器时钟源，必须同步更新 PWM_MAX_COMPARE。
 */
#define PWM_FREQ_HZ       20000 /* PWM 载波频率 */
#define PWM_MAX_COMPARE   2000  /* 占空比最大比较值 = TimerClock / 20000 */

void pwm_init(GPTIMER_Regs *htim);
void pwm_set_compare_ch0(GPTIMER_Regs *htim, uint16_t compare);
void pwm_set_compare_ch1(GPTIMER_Regs *htim, uint16_t compare);

#endif
