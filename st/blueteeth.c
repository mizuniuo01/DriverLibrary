#include "blueteeth.h"

static Blueteeth_Func_Struct blueteethInfo;

// 蓝牙指令字典与回调
// 此为示例
// static void function_name(void) { /*实现具体逻辑*/ }

static const CommandMap_Struct cmdTable[] = 
{
    // 此为示例
    // {"指令", function_name}
};

/**
 * @brief 根据解析出的命令字符串在指令字典中查找并执行对应的回调函数
 * @param cmdString 字符串指令
 * @retval None
 */
static void Process_Bluetooth_Command(const char *cmdString)
{
    for (uint8_t i = 0; i < CMD_TABLE_SIZE; i++)
    {
        if (strcmp(cmdString, cmdTable[i].cmdString) == 0)
        {
            cmdTable[i].cmdHandler();
            return;
        }
    }
}

/**
 * @brief 蓝牙模块初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void Blueteeth_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return;
    }

    blueteethInfo.huart = huart;

    blueteethInfo.rx_read_pos = 0;
    blueteethInfo.rx_write_pos = 0;
    blueteethInfo.tx_read_pos = 0;
    blueteethInfo.tx_write_pos = 0;
    blueteethInfo.is_tx_busy = 0;
    blueteethInfo.rxState = STATE_WAIT_HEADER;
    blueteethInfo.frameIndex = 0;

    HAL_UARTEx_ReceiveToIdle_DMA(blueteethInfo.huart, blueteethInfo.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
}

/**
 * @brief 从发送队列搬运数据到DMA发送缓冲区并启动发送
 * @param None
 * @retval None 
 */
static void Blueteeth_DataTransmit(void)
{
    if (blueteethInfo.tx_write_pos == blueteethInfo.tx_read_pos) 
    {
        blueteethInfo.is_tx_busy = 0;
        return;
    }

    blueteethInfo.is_tx_busy = 1;
    uint16_t sendLen = 0;
    uint16_t lenToCopy = 0;

    if (blueteethInfo.tx_write_pos > blueteethInfo.tx_read_pos)
    {
        sendLen = blueteethInfo.tx_write_pos - blueteethInfo.tx_read_pos;
        lenToCopy = (sendLen > sizeof(blueteethInfo.dma_tx_buffer)) ? sizeof(blueteethInfo.dma_tx_buffer) : sendLen;
        memcpy(blueteethInfo.dma_tx_buffer, &blueteethInfo.txFifo[blueteethInfo.tx_read_pos], lenToCopy);
        blueteethInfo.tx_read_pos += lenToCopy;
        sendLen = lenToCopy;
    }
    else
    {
        sendLen = BLUETEETH_TX_FIFO_SIZE - blueteethInfo.tx_read_pos;
        lenToCopy = (sendLen > sizeof(blueteethInfo.dma_tx_buffer)) ? sizeof(blueteethInfo.dma_tx_buffer) : sendLen;
        memcpy(blueteethInfo.dma_tx_buffer, &blueteethInfo.txFifo[blueteethInfo.tx_read_pos], lenToCopy);
        
        blueteethInfo.tx_read_pos += lenToCopy;
        if (blueteethInfo.tx_read_pos >= BLUETEETH_TX_FIFO_SIZE) 
        {
            blueteethInfo.tx_read_pos = 0;
        }
        sendLen = lenToCopy;
    }

    HAL_UART_Transmit_DMA(blueteethInfo.huart, blueteethInfo.dma_tx_buffer, sendLen);
}

/**
 * @brief 填充发送队列并启动发送 (DMA+FIFO模式)
 * @param format 格式化字符串
 * @retval None
 */
void Blueteeth_Printf_DMA(const char *format, ...)
{   
    char tempBuffer[128];
    va_list args;
    
    va_start(args, format);
    uint16_t length = vsnprintf(tempBuffer, sizeof(tempBuffer), format, args);
    va_end(args);

    if (length == 0)
    {
        return;
    }

    uint32_t primaskBit = __get_PRIMASK();
    __disable_irq();

    for (uint16_t i = 0; i < length; i++)
    {
        uint16_t nextWritePos = (blueteethInfo.tx_write_pos + 1) % BLUETEETH_TX_FIFO_SIZE;
        if (nextWritePos != blueteethInfo.tx_read_pos)
        {
            blueteethInfo.txFifo[blueteethInfo.tx_write_pos] = tempBuffer[i];
            blueteethInfo.tx_write_pos = nextWritePos;
        }
    }

    __set_PRIMASK(primaskBit);
    
    if (blueteethInfo.is_tx_busy == 0)
    {
        Blueteeth_DataTransmit();
    }
    
}

/**
 * @brief 蓝牙屏幕封装指令
 * @param x x坐标
 * @param y y坐标
 * @param format 格式化字符串
 * @retval None
 */
void Blueteeth_Display(int x, int y, const char *format, ...)
{
    char tempBuffer[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(tempBuffer, sizeof(tempBuffer), format, args);
    va_end(args);

    Blueteeth_Printf_DMA("[display,%d,%d,%s]\r\n", x, y, tempBuffer);
}

/**
 * @brief 清屏命令封装
 * @param None
 * @retval None
 */
void Blueteeth_Clear(void)
{
    Blueteeth_Printf_DMA("[display-clear]\r\n");
}

/**
 * @brief 绘图指令封装
 * @param format 格式化字符串
 * @retval None
 */
void Blueteeth_Plot(const char *format, ...)
{
    char tempBuffer[128];
    va_list args;
    
    va_start(args, format);
    vsnprintf(tempBuffer, sizeof(tempBuffer), format, args);
    va_end(args);

    Blueteeth_Printf_DMA("[plot,%s]\r\n", tempBuffer);
}

/**
 * @brief DMA发送完成回调
 * @param huart 串口句柄指针
 * @retval None
 */
void Blueteeth_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || blueteethInfo.huart == NULL)
    {
        return;
    }

    if (huart->Instance == blueteethInfo.huart->Instance)
    {
        Blueteeth_DataTransmit();
    }
}

/**
 * @brief DMA接收空闲回调
 * @param huart 串口句柄指针
 * @param Size 接收数据长度
 * @retval None
 */
void Blueteeth_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == NULL || blueteethInfo.huart == NULL)
    {
        return;
    }

    if (huart->Instance == blueteethInfo.huart->Instance)
    {
        if (Size > 0)
        {
            for (uint16_t i = 0; i < Size; i++)
            {
                uint16_t nextWritePos = (blueteethInfo.rx_write_pos + 1) % BLUETEETH_RX_FIFO_SIZE;
                
                if (nextWritePos != blueteethInfo.rx_read_pos)
                {
                    blueteethInfo.rxFifo[blueteethInfo.rx_write_pos] = blueteethInfo.dma_rx_buffer[i];
                    blueteethInfo.rx_write_pos = nextWritePos;
                }
            }
        }

        memset(blueteethInfo.dma_rx_buffer, 0, BLUETEETH_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(blueteethInfo.huart, blueteethInfo.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 蓝牙数据包解析执行任务
 * @param None
 * @retval None
 */
void Blueteeth_Task(void)
{
    while (blueteethInfo.rx_read_pos != blueteethInfo.rx_write_pos)
    {
        uint8_t dataReadingPos = blueteethInfo.rxFifo[blueteethInfo.rx_read_pos];
        blueteethInfo.rx_read_pos = (blueteethInfo.rx_read_pos + 1) % BLUETEETH_RX_FIFO_SIZE;

        switch (blueteethInfo.rxState)
        {
            case STATE_WAIT_HEADER:
            {
                if (dataReadingPos == FRAME_HEADER)
                {
                    blueteethInfo.frameIndex = 0;
                    blueteethInfo.rxState = STATE_RECEIVING_DATA;
                }
                break;
            }

            case STATE_RECEIVING_DATA:
            {
                if (dataReadingPos == FRAME_TAIL)
                {
                    if (blueteethInfo.frameIndex < BLUETEETH_MAX_FRAME_LEN)
                    {
                        blueteethInfo.frame_buffer[blueteethInfo.frameIndex] = '\0';
                    }

                    Process_Bluetooth_Command((char *)blueteethInfo.frame_buffer);
                    blueteethInfo.rxState = STATE_WAIT_HEADER;
                }
                else if (dataReadingPos == FRAME_HEADER)
                {
                    blueteethInfo.frameIndex = 0;
                }
                else
                {
                    if (blueteethInfo.frameIndex < BLUETEETH_MAX_FRAME_LEN - 1)
                    {
                        blueteethInfo.frame_buffer[blueteethInfo.frameIndex++] = dataReadingPos;
                    }
                    else
                    {
                        blueteethInfo.rxState = STATE_WAIT_HEADER;
                    }
                }
                break;
            }
        }
    }
}