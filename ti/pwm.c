#include "pwm.h"

/**
 * @brief 初始化并使能对应的 PWM 定时器实例
 * @param htim 绑定的定时器句柄指针 (如 PWM_MOTOR_INST)
 * @retval None
 */
void PWM_Init(GPTIMER_Regs *htim)
{
    if (htim == NULL) return;

    // 启动对应的定时器计数器，使能 PWM 在通道上的持续载波输出
    DL_Timer_startCounter(htim);
}

/**
 * @brief 设置指定的 PWM 定时器中 Channel 0 的比较值(占空比)
 * @param htim 绑定的定时器句柄
 * @param compare 设置的计数值
 * @retval None
 */
void PWM_Set_Compare_CH0(GPTIMER_Regs *htim, uint16_t compare)
{
    if (htim == NULL) return;

    // 限制最高边界防止发生溢出异常或导致全通死区
    if (compare > PWM_MAX_COMPARE) 
    {
        compare = PWM_MAX_COMPARE;
    }
    
    // 原 HAL 中使用的 __HAL_TIM_SET_COMPARE 替换为 DriverLib 的捕捉比较寄存器赋值 API
    DL_Timer_setCaptureCompareValue(htim, compare, DL_TIMER_CC_0_INDEX);
}

/**
 * @brief 设置指定的 PWM 定时器中 Channel 1 的比较值(占空比)
 * @param htim 绑定的定时器句柄
 * @param compare 设置的计数值
 * @retval None
 */
void PWM_Set_Compare_CH1(GPTIMER_Regs *htim, uint16_t compare)
{
    if (htim == NULL) return;

    if (compare > PWM_MAX_COMPARE) 
    {
        compare = PWM_MAX_COMPARE;
    }
    
    DL_Timer_setCaptureCompareValue(htim, compare, DL_TIMER_CC_1_INDEX);
}