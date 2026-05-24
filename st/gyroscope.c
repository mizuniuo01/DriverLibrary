#include "gyroscope.h"


static Gyro_Func_Struct gyroInfo;               // 陀螺仪通信和帧解析相关状态
volatile uint8_t gyro_tick_flag = 0;            // 由 1ms 定时器触发，用于驱动陀螺增量计算
static float gyro_delta_accumulated = 0.0f;     // 累积累计的 yaw 变化量，等待摄像头新帧消费
static float gyro_frame_delta_yaw = 0.0f;       // 与当前摄像头帧对齐的前馈 yaw 增量
static float gyro_last_yaw = 0.0f;              // 上一次有效 yaw 用于差分计算
static uint8_t gyro_delta_initialized = 0;      // 是否已初始化 yaw 差分基准
Gyro_Data_Struct gyroData = {0.0f, 0.0f, 0.0f}; // 最新姿态角度数据

/**
 * @brief 姿态传感器模块初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void Gyro_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return;

    gyroInfo.huart = huart;
    gyroInfo.rx_read_pos = 0;
    gyroInfo.rx_write_pos = 0;
    gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
    gyroInfo.frameIndex = 0;
    gyro_delta_accumulated = 0.0f;
    gyro_frame_delta_yaw = 0.0f;
    gyro_last_yaw = 0.0f;
    gyro_delta_initialized = 0;

    HAL_UARTEx_ReceiveToIdle_DMA(gyroInfo.huart, gyroInfo.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
}

/**
 * @brief 姿态传感器的DMA空闲中断接收回调
 * @param huart 串口句柄指针
 * @param Size 接收到的数据长度
 * @retval None
 */
void Gyro_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == NULL || gyroInfo.huart == NULL) return;

    if (huart->Instance == gyroInfo.huart->Instance)
    {
        if (Size > 0)
        {
            for (uint16_t i = 0; i < Size; i++)
            {
                uint16_t nextWritePos = (gyroInfo.rx_write_pos + 1) % GYRO_RX_FIFO_SIZE;

                if (nextWritePos != gyroInfo.rx_read_pos)
                {
                    gyroInfo.rxFifo[gyroInfo.rx_write_pos] = gyroInfo.dma_rx_buffer[i];
                    gyroInfo.rx_write_pos = nextWritePos;
                }
            }
        }

        memset(gyroInfo.dma_rx_buffer, 0, GYRO_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(gyroInfo.huart, gyroInfo.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 姿态传感器数据解析任务
 * @param None
 * @retval None
 */
void Gyro_Task(void)
{
    while (gyroInfo.rx_read_pos != gyroInfo.rx_write_pos)
    {
        uint8_t dataReadingPos = gyroInfo.rxFifo[gyroInfo.rx_read_pos];
        gyroInfo.rx_read_pos = (gyroInfo.rx_read_pos + 1) % GYRO_RX_FIFO_SIZE;

        switch (gyroInfo.rxState)
        {
            case GYRO_STATE_WAIT_HEADER:
            {
                if (dataReadingPos == GYRO_FRAME_HEADER)
                {
                    gyroInfo.frame_buffer[0] = dataReadingPos;
                    gyroInfo.frameIndex = 1;
                    gyroInfo.rxState = GYRO_STATE_WAIT_TYPE;
                }
                break;
            }

            case GYRO_STATE_WAIT_TYPE:
            {
                if (dataReadingPos == GYRO_TYPE_ANGLE)
                {
                    gyroInfo.frame_buffer[1] = dataReadingPos;
                    gyroInfo.frameIndex = 2;
                    gyroInfo.rxState = GYRO_STATE_RECEIVING_DATA;
                }
                else if (dataReadingPos == GYRO_FRAME_HEADER)
                {
                    gyroInfo.frame_buffer[0] = dataReadingPos;
                    gyroInfo.frameIndex = 1;
                }
                else
                {
                    gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
                }
                break;
            }

            case GYRO_STATE_RECEIVING_DATA:
            {
                gyroInfo.frame_buffer[gyroInfo.frameIndex++] = dataReadingPos;

                if (gyroInfo.frameIndex >= GYRO_FRAME_LEN)
                {
                    uint8_t checkSum = 0;
                    for (uint8_t i = 0; i < GYRO_FRAME_LEN - 1; i++)
                    {
                        checkSum += gyroInfo.frame_buffer[i];
                    }

                    if (checkSum == gyroInfo.frame_buffer[GYRO_FRAME_LEN - 1])
                    {
                        int16_t rollRaw = (int16_t)((gyroInfo.frame_buffer[3] << 8) | gyroInfo.frame_buffer[2]);
                        int16_t pitchRaw = (int16_t)((gyroInfo.frame_buffer[5] << 8) | gyroInfo.frame_buffer[4]);
                        int16_t yawRaw = (int16_t)((gyroInfo.frame_buffer[7] << 8) | gyroInfo.frame_buffer[6]);

                        gyroData.roll = (float)rollRaw / 32768.0f * 180.0f;
                        gyroData.pitch = (float)pitchRaw / 32768.0f * 180.0f;
                        gyroData.yaw = (float)yawRaw / 32768.0f * 180.0f;
                    }

                    gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
                }
                break;
            }
        }
    }

    if (gyro_tick_flag != 0)
    {
        gyro_tick_flag = 0;
        Gyro_UpdateDelta();
    }
}

/**
 * @brief 更新陀螺仪偏航角增量
 * @param None
 * @retval None
 */
void Gyro_UpdateDelta(void)
{
    if (!gyro_delta_initialized)
    {
        gyro_last_yaw = gyroData.yaw;
        gyro_delta_accumulated = 0.0f;
        gyro_frame_delta_yaw = 0.0f;
        gyro_delta_initialized = 1;
        return;
    }

    float current_yaw = gyroData.yaw;
    float delta = current_yaw - gyro_last_yaw;

    if (delta > 180.0f)
    {
        delta -= 360.0f;
    }
    else if (delta < -180.0f)
    {
        delta += 360.0f;
    }

    if (delta > GYRO_MAX_DELTA_DEG)
    {
        delta = GYRO_MAX_DELTA_DEG;
    }
    else if (delta < -GYRO_MAX_DELTA_DEG)
    {
        delta = -GYRO_MAX_DELTA_DEG;
    }

    // 累积 10ms 级别的偏航变化量，等待摄像头新帧时使用
    gyro_delta_accumulated += delta;
    gyro_last_yaw = current_yaw;
}

/**
 * @brief 处理摄像头新帧到达后的陀螺仪增量同步
 * @param None
 * @retval None
 */
void Gyro_On_Camera_Frame(void)
{
    // 将累积值转为当前摄像头帧的前馈量
    gyro_frame_delta_yaw = gyro_delta_accumulated;
    gyro_delta_accumulated = 0.0f;
}

/**
 * @brief 获取当前陀螺仪偏航角增量
 * @param None
 * @retval 当前增量，单位：度
 */
float Gyro_Get_DeltaYaw(void)
{
    return gyro_frame_delta_yaw;
}

/**
 * @brief 姿态传感器错误回调
 * @param huart 串口句柄指针
 * @retval None
 */
void Gyro_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || gyroInfo.huart == NULL) return;

    if (huart->Instance == gyroInfo.huart->Instance)
    {
        gyroInfo.rx_read_pos = 0;
        gyroInfo.rx_write_pos = 0;
        gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
        gyroInfo.frameIndex = 0;
        gyro_delta_initialized = 0;
        gyro_delta_accumulated = 0.0f;
        gyro_frame_delta_yaw = 0.0f;
        memset(gyroInfo.dma_rx_buffer, 0, GYRO_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(gyroInfo.huart, gyroInfo.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
    }
}