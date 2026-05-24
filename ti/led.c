#include "led.h"

/**
 * @brief 控制 LED1 的亮灭点亮状态
 * @param state 1代表点亮，0代表熄灭
 * @retval None
 */
void LED1_Set_State(uint8_t state)
{
    if (state == 1) {
        DL_GPIO_setPins(LED_LED1_PORT, LED_LED1_PIN);
    } else {
        DL_GPIO_clearPins(LED_LED1_PORT, LED_LED1_PIN);
    }
}

/**
 * @brief 翻转 LED1 的当前状态
 * @param None
 * @retval None
 */
void LED1_Toggle(void)
{
    DL_GPIO_togglePins(LED_LED1_PORT, LED_LED1_PIN);
}

/**
 * @brief 控制 LED2 的亮灭点亮状态
 * @param state 1代表点亮，0代表熄灭
 * @retval None
 */
void LED2_Set_State(uint8_t state)
{
    if (state == 1) {
        DL_GPIO_setPins(LED_LED2_PORT, LED_LED2_PIN);
    } else {
        DL_GPIO_clearPins(LED_LED2_PORT, LED_LED2_PIN);
    }
}

/**
 * @brief 翻转 LED2 的当前状态
 * @param None
 * @retval None
 */
void LED2_Toggle(void)
{
    DL_GPIO_togglePins(LED_LED2_PORT, LED_LED2_PIN);
}