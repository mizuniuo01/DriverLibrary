/**
 * @file    pwm.c
 * @brief   PWM 输出模块（定时器比较值控制）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：PWM 定时器已在 SysConfig 中配置
 * @note    无内部状态，通过传入的定时器句柄直接操作硬件
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * PWM 比较值薄封装，供 motor 模块调用。不独立使用。
 *
 * 配合 DRV8874：定时器 CH0/CH1 分别输出 PWM 到两片 DRV8874
 * 的 IN1 引脚，IN2 由 motor 模块的 GPIO 控制方向。
 *
 * 参数推导（以 20kHz 载波频率为基准）：
 *   PWM_FREQ_HZ = 20000
 *   PWM_MAX_COMPARE = 定时器时钟频率 / 20000
 *   例：SysConfig 中定时器时钟 40MHz → CC = 40000000/20000 = 2000
 *
 * SysConfig 配置要点：
 * - 定时器模式：PWM（Up count mode）
 * - 定时器时钟源频率决定 CC 值：CC = 时钟频率 / 20000
 * - 若修改时钟源，必须同步更新 pwm.h 中的 PWM_MAX_COMPARE
 * - 两个通道 CH0/CH1 使能，各自绑定到对应电机引脚
 *
 * ── 初始化 ──
 *
 * pwm_init(PWM_MOTOR_INST);  // 启动计数器，PWM 开始输出
 *
 * ── motor 模块调用（不直接使用）──
 *
 * pwm_set_compare_ch0(htim, 1000);  // 左电机 50% 占空比
 * pwm_set_compare_ch1(htim, 1500);  // 右电机 75% 占空比
 */

#include "pwm.h"

/**
 * @brief  启动 PWM 定时器计数器
 * @param  htim  定时器句柄指针
 * @retval DRV_OK 成功，DRV_ERR_PARAM 参数非法
 */
drv_err_t pwm_init(GPTIMER_Regs *htim)
{
    if (!htim) {
        return DRV_ERR_PARAM;
    }

    DL_Timer_startCounter(htim);

    return DRV_OK;
}

/**
 * @brief  设置 PWM 通道 0 的比较值（占空比）
 * @param  htim     定时器句柄指针
 * @param  compare  比较值（0~PWM_MAX_COMPARE），超限自动钳位
 * @retval 无
 */
void pwm_set_compare_ch0(GPTIMER_Regs *htim, uint16_t compare)
{
    if (!htim) {
        return;
    }

    if (compare > PWM_MAX_COMPARE) {
        compare = PWM_MAX_COMPARE;
    }

    DL_Timer_setCaptureCompareValue(htim, compare, DL_TIMER_CC_0_INDEX);
}

/**
 * @brief  设置 PWM 通道 1 的比较值（占空比）
 * @param  htim     定时器句柄指针
 * @param  compare  比较值（0~PWM_MAX_COMPARE），超限自动钳位
 * @retval 无
 */
void pwm_set_compare_ch1(GPTIMER_Regs *htim, uint16_t compare)
{
    if (!htim) {
        return;
    }

    if (compare > PWM_MAX_COMPARE) {
        compare = PWM_MAX_COMPARE;
    }

    DL_Timer_setCaptureCompareValue(htim, compare, DL_TIMER_CC_1_INDEX);
}
