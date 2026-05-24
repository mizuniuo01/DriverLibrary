#include "laser.h"

/**
 * @brief 开启激光笔
 * @param None
 * @retval None
 */
void LASER_ON(void)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_SET);
}

/**
 * @brief 关闭激光笔
 * @param None
 * @retval None
 */
void LASER_OFF(void)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_11, GPIO_PIN_RESET);
}

/**
 * @brief 翻转激光笔状态
 * @param None
 * @retval None
 */
void LASER_TOGGLE(void)
{
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);
}