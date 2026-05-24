#include "buzzer.h"

/**
 * @brief 打开蜂鸣器
 * @retval None
 */
void Buzzer_ON(void)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
}

/**
 * @brief 关闭蜂鸣器
 * @retval None
 */
void Buzzer_OFF(void)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 翻转蜂鸣器状态
 * @retval None
 */
void Buzzer_TOGGLE(void)
{
    HAL_GPIO_TogglePin(Buzzer_GPIO_Port, Buzzer_Pin);
}