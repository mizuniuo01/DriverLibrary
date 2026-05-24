#include "encoder.h"

Encoder_Data_t encoderData = {0, 0};

/**
 * @brief 编码器外设统一初始化
 * @param htim_left 左侧编码器定时器句柄
 * @param htim_right 右侧编码器定时器句柄
 * @retval None
 */
void Encoder_Init(TIM_HandleTypeDef *htim_left, TIM_HandleTypeDef *htim_right)
{
    if (htim_left == NULL || htim_right == NULL)
    {
        return;
    }

    // 复位定时器硬件计数值
    __HAL_TIM_SET_COUNTER(htim_left, 0);
    __HAL_TIM_SET_COUNTER(htim_right, 0);
    
    // 使能对应定时器的底层正交编码器接口模式
    HAL_TIM_Encoder_Start(htim_left, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(htim_right, TIM_CHANNEL_ALL);
}

/**
 * @brief 获取左侧编码器计数值增量
 * @param htim 绑定的定时器句柄
 * @retval none
 */
void Encoder_Get_Left(TIM_HandleTypeDef *htim)
{
    static uint16_t last_count_left = 0;
    uint16_t current_count = __HAL_TIM_GET_COUNTER(htim);
    
    // 利用无符号转有符号的截断机制，实现硬件溢出自适应处理
    int16_t diff_value = (int16_t)(current_count - last_count_left);
    // 读数取反适应实际轮胎方向
    encoderData.left_val = -diff_value; // 供外部访问
    last_count_left = current_count;
}

/**
 * @brief 获取右侧编码器计数值增量
 * @param htim 绑定的定时器句柄
 * @retval none
 */
void Encoder_Get_Right(TIM_HandleTypeDef *htim)
{
    static uint16_t last_count_right = 0;
    uint16_t current_count = __HAL_TIM_GET_COUNTER(htim);
    
    int16_t diff_value = (int16_t)(current_count - last_count_right);
    encoderData.right_val = diff_value; // 供外部访问
    last_count_right = current_count;
}