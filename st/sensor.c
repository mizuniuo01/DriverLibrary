#include "sensor.h"

// 读取灰度传感器数据结构定义与句柄
static I2C_HandleTypeDef *sensor_hi2c = NULL;
static uint8_t sensor_rx_buffer[1];
static volatile uint8_t sensor_processed_data = 0;
static volatile uint8_t dma_busy = 0;
volatile uint8_t sensor_tick_flag = 0;

/**
 * @brief 灰度传感器硬件I2C初始化
 * @param hi2c 绑定的I2C硬件句柄指针
 * @retval None
 */
void Sensor_Init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) return;
    sensor_hi2c = hi2c;
    dma_busy = 0;
}

/**
 * @brief 获取 DMA 工作状态
 * @param None
 * @retval Sensor_State_t 当前传感器状态（忙/空闲）
 */
Sensor_State_t Sensor_GetState(void) 
{
    return dma_busy ? SENSOR_BUSY : SENSOR_IDLE;
}

/**
 * @brief 发起DMA请求读取传感器数字数据
 * @param None
 * @retval None
 */
void Sensor_RequestData_DMA(void)
{
    if (sensor_hi2c == NULL || dma_busy) return;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read_DMA(sensor_hi2c, SENSOR_I2C_ADDR, SENSOR_CMD_READ_DIG, I2C_MEMADD_SIZE_8BIT, sensor_rx_buffer, 1);

    if (status == HAL_OK) 
    {
        dma_busy = 1;
    } 
    else 
    {
        // 遇到通信阻塞主动释掉标志位
        dma_busy = 0;
    }
}

/**
 * @brief I2C DMA 内存读取完成中断回调
 * @param hi2c 触发中断的I2C句柄
 * @retval None
 */
void Sensor_RxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (sensor_hi2c == NULL) return;

    if (hi2c->Instance == sensor_hi2c->Instance)
    {
        dma_busy = 0;

        uint8_t raw_data = sensor_rx_buffer[0];
        uint8_t temp_data = 0;
        
        /* 
         * [统一标准约定 —— 低位在左/高位在右]
         * 物理层面：最左侧探头 -> bit 0，最右侧探头 -> bit 7 
         * 逻辑定义：0(白底/未触发) 1(黑线/触发)
         * 假设传感器硬件原始返回值的bit X正好对应从左到右的探头X，
         * 因为原始I2C数据低电平是黑 需要将其取反提取。
         */
        for (uint8_t i = 0; i < 8; i++)
        {
            // 原始引脚为低(0)时代表探测到黑线，此处将其转为正逻辑(1)
            if (((raw_data >> i) & 0x01) == 0)
            {
                // 直接让新数据的 bit i 等于探测到的 1
                // i = 0存到bit0 (最左), i = 7存到bit7 (最右)
                temp_data |= (1 << i);
            }
        }
        
        // 安全刷新系统应用层的全局存储变量
        sensor_processed_data = temp_data;
    }
}

/**
 * @brief DMA 发生通讯错误回调
 * @param hi2c 触发错误的I2C句柄
 * @retval None
 */
void Sensor_ErrorCallback(I2C_HandleTypeDef *hi2c) 
{
    if (sensor_hi2c && hi2c->Instance == sensor_hi2c->Instance) 
    {
        dma_busy = 0;  // 错误时重置忙标志 防止后续请求永远被拒绝
    }
}

/**
 * @brief 获取并在底层完成位序转换后的传感器数据
 * @param None
 * @retval uint8_t 转换后的位图数据
 */
uint8_t Sensor_ReadData(void)
{
    // 直接返回由 DMA 完成中断处理后的最新状态数据
    return sensor_processed_data;
}

/**
 * @brief 传感器轮询读取任务
 * @param None
 * @retval None
 */
void Sensor_Task(void)
{
    if (sensor_tick_flag == 1)
    {
        sensor_tick_flag = 0;
        Sensor_RequestData_DMA();
    }
}