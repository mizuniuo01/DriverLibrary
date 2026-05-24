#include "gyroscope.h"

static Gyro_Func_Struct gyroInfo;
Gyro_Data_Struct gyroData = {0.0f, 0.0f, 0.0f};
volatile uint8_t gyro_checkIdleFlag = 0;

/**
 * @brief 启动底层 DMA 的接收流程
 * @param pData 目标数据缓冲区指针
 * @param Size 数据目标长度
 * @retval None
 */
static void Start_DMA_Rx(uint8_t *pData, uint16_t Size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, (uint32_t)(&UART_GYROSCOPE_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, (uint32_t)pData);
    DL_DMA_setTransferSize(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, Size);
    DL_DMA_enableChannel(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
}

/**
 * @brief 姿态传感器模块初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void Gyro_Init(UART_Regs *huart)
{
    if (huart == NULL) return;

    DL_UART_Main_setRXInterruptTimeout(UART_GYROSCOPE_INST, 10);
    NVIC_EnableIRQ(UART_GYROSCOPE_INST_INT_IRQN);

    gyroInfo.huart = huart;
    
    gyroInfo.rx_read_pos = 0;
    gyroInfo.rx_write_pos = 0;
    gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
    gyroInfo.frameIndex = 0;
    
    Start_DMA_Rx(gyroInfo.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
}

/**
 * @brief 姿态传感器的 IDLE/DMA 接收回调
 * @param huart 串口句柄指针
 * @param Size 实际接收的有效数据长度
 * @retval None
 */
void Gyro_RxEventCallback(UART_Regs *huart, uint16_t Size)
{
    if (huart == NULL || gyroInfo.huart == NULL) return;

    if (huart == gyroInfo.huart)
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
        Start_DMA_Rx(gyroInfo.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 陀螺仪串口错误回调
 * @param huart 串口句柄指针
 * @retval None
 */
void Gyro_ErrorCallback(UART_Regs *huart)
{
    if (huart != gyroInfo.huart) return;

    gyroInfo.rxState = GYRO_STATE_WAIT_HEADER;
    gyroInfo.frameIndex = 0;
}

/**
 * @brief 姿态传感器数据解析任务
 * @param None
 * @retval None
 */
void Gyro_Task(void)
{
    // 软件IDLE：DMA余量连续两次相同则flush（MSPM0 RX_TIMEOUT硬件不可靠）
    if (gyro_checkIdleFlag)
    {
        gyro_checkIdleFlag = 0;
        static uint16_t last_remain = GYRO_DMA_RX_BUF_SIZE;
        uint16_t remain = DL_DMA_getTransferSize(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
        if (remain < GYRO_DMA_RX_BUF_SIZE)
        {
            if (remain == last_remain)
            {
                uint16_t rxLen = GYRO_DMA_RX_BUF_SIZE - remain;
                Gyro_RxEventCallback(UART_GYROSCOPE_INST, rxLen);
            }
            last_remain = remain;
        }
        else
        {
            last_remain = GYRO_DMA_RX_BUF_SIZE;
        }
    }

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
}