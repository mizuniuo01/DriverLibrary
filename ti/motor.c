#include "motor.h"

volatile int8_t motor_right_direction_sign = 1;

/**
 * @brief 初始化两路电机的使能与 PWM 控制源并唤醒驱动层
 * @param htim 将绑定主导这俩电机的基础定时器柄
 * @retval None
 */
void Motor_Init(void)
{
    // 拉高驱动芯片的休眠解除信号管脚，让电机得以脱离待机模式
    DL_GPIO_setPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_L_nSLEEP_PIN);
    DL_GPIO_setPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_R_nSLEEP_PIN);
}

/**
 * @brief 设置1号(左侧)电机，参数为直接PWM比较值
 * @param htim 驱动该组的绑带定时器实体
 * @param pwm_val PWM 比较值(-PWM_MAX_COMPARE ~ +PWM_MAX_COMPARE)
 * @retval None
 */
void Motor_Set_Speed_1(GPTIMER_Regs *htim, int16_t pwm_val)
{
    if (htim == NULL) return;

    if (pwm_val >= MOTOR_MIN_SPEED)
    {
        if (pwm_val > MOTOR_MAX_SPEED) pwm_val = MOTOR_MAX_SPEED;

        if (pwm_val < MOTOR_EFFECTIVE_MIN_SPEED) {
            PWM_Set_Compare_CH0(htim, 0);
            return;
        }

        DL_GPIO_setPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_L_PH_IN2_PIN);
        PWM_Set_Compare_CH0(htim, (uint16_t)pwm_val);
    }
    else
    {
        int16_t temp = -pwm_val;
        if (temp > MOTOR_MAX_SPEED) temp = MOTOR_MAX_SPEED;

        if (temp < MOTOR_EFFECTIVE_MIN_SPEED) {
            PWM_Set_Compare_CH0(htim, 0);
            return;
        }

        DL_GPIO_clearPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_L_PH_IN2_PIN);
        PWM_Set_Compare_CH0(htim, (uint16_t)temp);
    }
}

/**
 * @brief 设置2号(右侧)电机，参数为直接PWM比较值
 * @param htim 所归属硬件组发波主端句柄
 * @param pwm_val PWM 比较值(-PWM_MAX_COMPARE ~ +PWM_MAX_COMPARE)
 * @retval None
 */
void Motor_Set_Speed_2(GPTIMER_Regs *htim, int16_t pwm_val)
{
    if (htim == NULL) return;

    if (pwm_val >= MOTOR_MIN_SPEED)
    {
        if (pwm_val > MOTOR_MAX_SPEED) pwm_val = MOTOR_MAX_SPEED;

        if (pwm_val < MOTOR_EFFECTIVE_MIN_SPEED) {
            PWM_Set_Compare_CH1(htim, 0);
            return;
        }

        motor_right_direction_sign = 1;
        DL_GPIO_clearPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_R_PH_IN2_PIN);
        PWM_Set_Compare_CH1(htim, (uint16_t)pwm_val);
    }
    else
    {
        int16_t temp = -pwm_val;
        if (temp > MOTOR_MAX_SPEED) temp = MOTOR_MAX_SPEED;

        if (temp < MOTOR_EFFECTIVE_MIN_SPEED) {
            PWM_Set_Compare_CH1(htim, 0);
            return;
        }

        motor_right_direction_sign = -1;
        DL_GPIO_setPins(GPIO_MOTOR_PORT_PORT, GPIO_MOTOR_PORT_R_PH_IN2_PIN);
        PWM_Set_Compare_CH1(htim, (uint16_t)temp);
    }
}
