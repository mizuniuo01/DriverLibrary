#include "motor.h"

/**
 * @brief 电机初始化 唤醒DRV8874
 * @param htim 绑定的PWM定时器句柄
 * @retval None
 */
void Motor_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL) return;

    // 拉高 nSLEEP 唤醒两路电机驱动
    HAL_GPIO_WritePin(L_nSLEEP_GPIO_Port, L_nSLEEP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R_nSLEEP_GPIO_Port, R_nSLEEP_Pin, GPIO_PIN_SET);
}

/**
 * @brief 设定电机1的转速和方向
 * @param htim 绑定的PWM定时器句柄
 * @param speed 目标速度，带符号代表方向
 * @retval None
 */
void Motor_SetSpeed1(TIM_HandleTypeDef *htim, int16_t speed)
{
    if (htim == NULL) return;

    if (speed >= MOTOR_MIN_SPEED)
    {
        if (speed > MOTOR_MAX_SPEED) speed = MOTOR_MAX_SPEED;

        // PH/EN模式代码保留
//        // 正向：设置方向引脚 IN2(PH) 状态
//        HAL_GPIO_WritePin(GPIOE, L_PH_IN2_Pin, GPIO_PIN_RESET);      
//        // 更新PWM载波占空比输出 (IN1)
//        PWM_SetCompare3(htim, (uint16_t)(speed * 84));

        // IN/IN模式 - 左轮正向
        // IN2(GPIO)长低，IN1(PWM)发正常正向占空比，高电平全压推，低电平短路刹车以保留电流
        HAL_GPIO_WritePin(GPIOE, L_PH_IN2_Pin, GPIO_PIN_SET);
        PWM_SetCompare3(htim, (uint16_t)(speed * 84));
    }
    else
    {
        int16_t temp = -speed;
        if (temp > MOTOR_MAX_SPEED) temp = MOTOR_MAX_SPEED;

//        // 反向：设置方向引脚 IN2(PH) 状态翻转
//        HAL_GPIO_WritePin(GPIOE, L_PH_IN2_Pin, GPIO_PIN_SET);       
//        // 更新PWM载波占空比输出 (IN1)
//        PWM_SetCompare3(htim, (uint16_t)(temp * 84));

        // IN/IN模式 - 左轮反向
        // IN2(GPIO)长高，IN1(PWM)发补码占空比，实现电学与推力一致的倒退
        HAL_GPIO_WritePin(GPIOE, L_PH_IN2_Pin, GPIO_PIN_RESET); 
        PWM_SetCompare3(htim, (uint16_t)((MOTOR_MAX_SPEED - temp) * 84));
    }
}

/**
 * @brief 设定电机2的转速和方向
 * @param htim 绑定的PWM定时器句柄
 * @param speed 目标速度，带符号代表方向
 * @retval None
 */
void Motor_SetSpeed2(TIM_HandleTypeDef *htim, int16_t speed)
{
    if (htim == NULL) return;

    if (speed >= MOTOR_MIN_SPEED)
    {
        if (speed > MOTOR_MAX_SPEED) speed = MOTOR_MAX_SPEED;

        // PH/EN模式代码保留
//        // 正向：设置方向引脚 IN2(PH) 状态
//        HAL_GPIO_WritePin(GPIOE, R_PH_IN2_Pin, GPIO_PIN_SET);   
//        // 更新PWM载波占空比输出
//        PWM_SetCompare4(htim, (uint16_t)(speed * 84));

        // IN/IN模式 - 正向
        // IN2(GPIO)长高，IN1(PWM)发补码占空比，低电平期间维持短路刹车以保留线圈电流
        HAL_GPIO_WritePin(GPIOE, R_PH_IN2_Pin, GPIO_PIN_SET); 
        PWM_SetCompare4(htim, (uint16_t)((MOTOR_MAX_SPEED - speed) * 84));
    }
    else
    {
        int16_t temp = -speed;
        if (temp > MOTOR_MAX_SPEED) temp = MOTOR_MAX_SPEED;

//        // 反向：设置方向引脚 IN2(PH) 状态翻转
//        HAL_GPIO_WritePin(GPIOE, R_PH_IN2_Pin, GPIO_PIN_RESET);    
//        // 更新PWM载波占空比输出
//        PWM_SetCompare4(htim, (uint16_t)(temp * 84));

        // IN/IN模式 - 反向
        // IN2(GPIO)长低，IN1(PWM)发正常占空比，高电平期间全压推，低电平期间短路刹车
        HAL_GPIO_WritePin(GPIOE, R_PH_IN2_Pin, GPIO_PIN_RESET);
        PWM_SetCompare4(htim, (uint16_t)(temp * 84));
    }
}