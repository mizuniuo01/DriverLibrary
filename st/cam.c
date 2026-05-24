#include "cam.h"

static Cam_Func_Struct camInfo;
Cam_Data_Struct camData = {0, 0, 0};    // 摄像头数据结构体实例，根据实际协议定义进行填充
volatile uint8_t cam_frame_ready = 0;

/**
 * @brief 摄像头模块初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void Cam_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return;
    }

    // 绑定外部传入的UART句柄用于底层交互
    camInfo.huart = huart;
    cam_frame_ready = 0;
    
    // 软状态机与环形缓冲区(FIFO)管理指针重置初始化
    camInfo.rx_read_pos = 0;
    camInfo.rx_write_pos = 0;
    camInfo.rxState = CAM_STATE_WAIT_HEADER;
    camInfo.frameIndex = 0;

    // 清空上次状态并开启DMA空闲中断接收机制
    HAL_UARTEx_ReceiveToIdle_DMA(camInfo.huart, camInfo.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
}

/**
 * @brief 摄像头的DMA接收回调数据搬运逻辑
 * @param huart 串口句柄指针
 * @param Size 接收到的数据长度
 * @retval None
 */
void Cam_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == NULL || camInfo.huart == NULL)
    {
        return;
    }

    if (huart->Instance == camInfo.huart->Instance)
    {
        if (Size > 0)
        {
            // 将本次DMA拿到的不定长突发数据，遍历压入应用层的循环FIFO中
            for (uint16_t i = 0; i < Size; i++)
            {
                uint16_t nextWritePos = (camInfo.rx_write_pos + 1) % CAM_RX_FIFO_SIZE;
                // 检测FIFO队列是否满载越界，防止旧数据被覆盖冲刷
                if (nextWritePos != camInfo.rx_read_pos)
                {
                    camInfo.rxFifo[camInfo.rx_write_pos] = camInfo.dma_rx_buffer[i];
                    camInfo.rx_write_pos = nextWritePos;
                }
            }
        }

        // DMA处理完毕后彻底重置硬件缓冲区并再次开启监听
        memset(camInfo.dma_rx_buffer, 0, CAM_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(camInfo.huart, camInfo.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 摄像头数据解析任务
 * @param None
 * @retval None
 */
void Cam_Task(void)
{
    // 只要环形队列里有未读数据，就持续提取进行状态机解析
    while (camInfo.rx_read_pos != camInfo.rx_write_pos)
    {
        // 从FIFO提取单个字节流进行消费
        uint8_t dataReadingPos = camInfo.rxFifo[camInfo.rx_read_pos];
        camInfo.rx_read_pos = (camInfo.rx_read_pos + 1) % CAM_RX_FIFO_SIZE;

        // 流式帧解析微状态机
        switch (camInfo.rxState)
        {
            case CAM_STATE_WAIT_HEADER:
            {
                // 等待帧头校验达成
                if (dataReadingPos == CAM_FRAME_HEADER)
                {
                    camInfo.frameIndex = 0;
                    camInfo.rxState = CAM_STATE_RECEIVING_DATA;
                }
                break;
            }

            case CAM_STATE_RECEIVING_DATA:
            {
                // 判断是否遇到一帧的终止符标志位
                if (dataReadingPos == CAM_FRAME_TAIL)
                {   
                    if (camInfo.frameIndex < CAM_MAX_FRAME_LEN)
                    {
                        camInfo.frame_buffer[camInfo.frameIndex] = '\0';
                    }

                    

                    // 这里写具体解析逻辑
                    


                    cam_frame_ready = 1;
                    camInfo.rxState = CAM_STATE_WAIT_HEADER;
                }
                else if (dataReadingPos == CAM_FRAME_HEADER)
                {
                    // 数据域内意外遇到新帧头，强制复位重算
                    camInfo.frameIndex = 0;
                }
                else
                {
                    // 正常的有效荷载数据放入用户帧缓存，并附带越界保护
                    if (camInfo.frameIndex < CAM_MAX_FRAME_LEN)
                    {
                        camInfo.frame_buffer[camInfo.frameIndex++] = dataReadingPos;
                    }
                    else 
                    {
                        // 单帧内容超规，强制抛弃当前包并恢复初始状态
                        camInfo.rxState = CAM_STATE_WAIT_HEADER;
                    }
                }
                break;
            }
        }
    }
}