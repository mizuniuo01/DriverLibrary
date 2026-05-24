#include "key.h"

/**
 * @brief 按键扫描函数 (含阻塞消抖)
 * @param None
 * @retval uint8_t 触发的按键编号，0表示无按键
 */
uint8_t Key_GetNum(void)
{
    uint8_t keyNum = 0;

    if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY1_GPIO_PIN) == KEY_PRESSED_STATE)
    {
        HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        
        while (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY1_GPIO_PIN) == KEY_PRESSED_STATE)
        {
            HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        }
        
        keyNum = 1;
    }

    if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY2_GPIO_PIN) == KEY_PRESSED_STATE)
    {
        HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        
        while (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY2_GPIO_PIN) == KEY_PRESSED_STATE)
        {
            HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        }
        
        keyNum = 2;
    }

    if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY3_GPIO_PIN) == KEY_PRESSED_STATE)
    {
        HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        
        while (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY3_GPIO_PIN) == KEY_PRESSED_STATE)
        {
            HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        }
        
        keyNum = 3;
    }

    if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY4_GPIO_PIN) == KEY_PRESSED_STATE)
    {
        HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        
        while (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY4_GPIO_PIN) == KEY_PRESSED_STATE)
        {
            HAL_Delay(KEY_DEBOUNCE_DELAY_MS);
        }
        
        keyNum = 4;
    }

    return keyNum;
}