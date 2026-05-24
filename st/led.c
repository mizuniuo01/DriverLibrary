#include "led.h"

/**
 * @brief 点亮LED1
 * @param None
 * @retval None
 */
void LED1_ON(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief 熄灭LED1
 * @param None
 * @retval None
 */
void LED1_OFF(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 翻转LED1状态
 * @param None
 * @retval None
 */
void LED1_TOGGLE(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED1_GPIO_PIN);
}

/**
 * @brief 点亮LED2
 * @param None
 * @retval None
 */
void LED2_ON(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief 熄灭LED2
 * @param None
 * @retval None
 */
void LED2_OFF(void)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 翻转LED2状态
 * @param None
 * @retval None
 */
void LED2_TOGGLE(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED2_GPIO_PIN);
}

/**
 * @brief 系统运行指示灯 LED1每1秒闪烁一次
 * @param None
 * @retval None
 */
void LED_ShowSystemStatus(void)
{
    static uint32_t last_toggle_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_toggle_time >= 1000) // 每1000ms翻转一次
    {
        LED1_TOGGLE();
        last_toggle_time = current_time;
    }
}