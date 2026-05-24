#include "pwm.h"

/**
 * @brief 初始化并启动PWM通道
 * @param htim 绑定的定时器句柄
 * @retval None
 */
void PWM_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return;
    }

    // 启动对应定时器的底层硬件PWM输出功能
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_4);

    // 如果是高级定时器(如TIM1) 为了保险强制使能MOE
    if (htim->Instance == TIM1) 
    {
        HAL_TIMEx_PWMN_Start(htim, TIM_CHANNEL_3);
        __HAL_TIM_MOE_ENABLE(htim);
    }
}

/**
 * @brief 设置定时器通道3的PWM比较值
 * @param htim 绑定的定时器句柄
 * @param compare 比较值
 * @retval None
 */
void PWM_SetCompare3(TIM_HandleTypeDef *htim, uint16_t compare)
{
    if (htim == NULL)
    {
        return;
    }

    // 限制最大比较值，防止占空比溢出或导致硬件保护
    if (compare > PWM_MAX_COMPARE) 
    {
        compare = PWM_MAX_COMPARE;
    }
    
    // 写入定时器比较寄存器，立即更新占空比
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, compare);
}

/**
 * @brief 设置定时器通道4的PWM比较值
 * @param htim 绑定的定时器句柄
 * @param compare 比较值
 * @retval None
 */
void PWM_SetCompare4(TIM_HandleTypeDef *htim, uint16_t compare)
{
    if (htim == NULL)
    {
        return;
    }

    // 限制最大比较值，防止占空比溢出或导致硬件保护
    if (compare > PWM_MAX_COMPARE) 
    {
        compare = PWM_MAX_COMPARE;
    }
    
    // 写入定时器比较寄存器，立即更新占空比
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, compare);
}