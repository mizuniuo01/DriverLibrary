#include "cam.h"

static Cam_Func_Struct camInfo;
volatile Cam_Data_Struct camData = {0, 0, 0, 0};  // 摄像头数据实例，具体内容根据实际项目需求定义

/**
 * @brief 内部启动DMA接收核心配置，设置源和目标内存位置指针
 * @param pData DMA目标缓冲区指针
 * @param Size 请求的数据量
 * @retval None
 */
static void Start_DMA_Rx(uint8_t *pData, uint16_t Size)
{
    DL_DMA_disableChannel(DMA, DMA_CAM_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CAM_RX_CHAN_ID, (uint32_t)(&UART_CAM_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CAM_RX_CHAN_ID, (uint32_t)pData);
    DL_DMA_setTransferSize(DMA, DMA_CAM_RX_CHAN_ID, Size);
    DL_DMA_enableChannel(DMA, DMA_CAM_RX_CHAN_ID);
}

/**
 * @brief 摄像头模块初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void Cam_Init(UART_Regs *huart)
{
    if (huart == NULL) return;
    
    DL_UART_Main_setRXInterruptTimeout(UART_CAM_INST, 10);
    NVIC_EnableIRQ(UART_CAM_INST_INT_IRQN);

    camInfo.huart = huart;
    
    camInfo.rx_read_pos = 0;
    camInfo.rx_write_pos = 0;
    camInfo.rxState = CAM_STATE_WAIT_HEADER;
    camInfo.frameIndex = 0;


    // 清空上次状态并开启DMA空闲中断接收机制
    memset(camInfo.dma_rx_buffer, 0, CAM_DMA_RX_BUF_SIZE);
    Start_DMA_Rx(camInfo.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
}

/**
 * @brief 摄像头的DMA接收回调数据搬运逻辑
 * @param huart 串口句柄指针
 * @param Size 接收到的数据长度
 * @retval None
 */
void Cam_RxEventCallback(UART_Regs *huart, uint16_t Size)
{
    if (huart == NULL || camInfo.huart == NULL)
    {
        return;
    }

    if (huart == camInfo.huart)
    {
        if (Size > 0)
        {
            for (uint16_t i = 0; i < Size; i++)
            {
                uint16_t nextWritePos = (camInfo.rx_write_pos + 1) % CAM_RX_FIFO_SIZE;
                if (nextWritePos != camInfo.rx_read_pos)
                {
                    camInfo.rxFifo[camInfo.rx_write_pos] = camInfo.dma_rx_buffer[i];
                    camInfo.rx_write_pos = nextWritePos;
                }
            }
        }

        memset(camInfo.dma_rx_buffer, 0, CAM_DMA_RX_BUF_SIZE);
        Start_DMA_Rx(camInfo.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 摄像头串口错误回调
 * @param huart 串口句柄指针
 * @retval None
 */
void Cam_ErrorCallback(UART_Regs *huart)
{
    if (huart != camInfo.huart) return;

    camInfo.rxState = CAM_STATE_WAIT_HEADER;
    camInfo.frameIndex = 0;
}

/**
 * @brief 摄像头串流数据解析状态机系统任务，持续运作
 * @param None
 * @retval None
 */
void Cam_Task(void)
{
    while (camInfo.rx_read_pos != camInfo.rx_write_pos)
    {
        uint8_t dataReadingPos = camInfo.rxFifo[camInfo.rx_read_pos];
        camInfo.rx_read_pos = (camInfo.rx_read_pos + 1) % CAM_RX_FIFO_SIZE;

        switch (camInfo.rxState)
        {
            case CAM_STATE_WAIT_HEADER:
            {
                if (dataReadingPos == CAM_FRAME_HEADER)
                {
                    camInfo.frameIndex = 0;
                    camInfo.rxState = CAM_STATE_RECEIVING_DATA;
                }
                break;
            }

            case CAM_STATE_RECEIVING_DATA:
            {
                if (dataReadingPos == CAM_FRAME_TAIL)
                {   
                /*************************数据解析*************************/
                    if (camInfo.frameIndex < CAM_MAX_FRAME_LEN)
                    {
                        camInfo.frame_buffer[camInfo.frameIndex] = '\0';
                    }

                    // 解析数据帧内容
                    // 根据实际数据格式进行解析
                    
                    camInfo.rxState = CAM_STATE_WAIT_HEADER;
                }
                /*********************************************************/
                else if (dataReadingPos == CAM_FRAME_HEADER)
                {
                    camInfo.frameIndex = 0;
                }
                else
                {
                    if (camInfo.frameIndex < CAM_MAX_FRAME_LEN)
                    {
                        camInfo.frame_buffer[camInfo.frameIndex++] = dataReadingPos;
                    }
                    else 
                    {
                        camInfo.rxState = CAM_STATE_WAIT_HEADER;
                    }
                }
                break;
            }
        }
    }
}
