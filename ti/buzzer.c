#include "buzzer.h"

/**
 * @brief 打开蜂鸣器
 * @retval None
 */
void Buzzer_ON(void)
{
    DL_GPIO_setPins(BUZZER_PORT, BUZZER_BUZZER1_PIN);
}

/**
 * @brief 关闭蜂鸣器
 * @retval None
 */
void Buzzer_OFF(void)
{
    DL_GPIO_clearPins(BUZZER_PORT, BUZZER_BUZZER1_PIN);
}

/**
 * @brief 翻转蜂鸣器状态
 * @retval None
 */
void Buzzer_TOGGLE(void)
{
    DL_GPIO_togglePins(BUZZER_PORT, BUZZER_BUZZER1_PIN);
}