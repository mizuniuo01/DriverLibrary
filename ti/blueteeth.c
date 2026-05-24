/*
 * 文件: blueteeth.c
 * 
 *
 *
*/

#include "blueteeth.h"

static Blueteeth_Func_Struct blueteethInfo;
volatile uint8_t blueteeth_checkIdleFlag;


// 蓝牙指令字典与回调
// 示例
// static void function_name(void) { /* 处理逻辑 */ }
// 延续指令的处理函数

static const CommandMap_Struct cmdTable[] = 
{
    // 示例
    // {"指令", function_name}
};

/**
 * @brief 内部指令查询执行通道
 * @param cmdString 收到的指令头字符串指针
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
 * @brief 启动底层 DMA 的接收流程
 * @param pData 目标数据缓冲区指针
 * @param Size 数据目标长度
 * @retval None
 */
static void Start_DMA_Rx(uint8_t *pData, uint16_t Size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID, (uint32_t)(&UART_BLUETEETH_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID, (uint32_t)pData);
    DL_DMA_setTransferSize(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID, Size);
    DL_DMA_enableChannel(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
}

/**
 * @brief 启动底层 DMA 的发送流程
 * @param pData 源数据缓冲区指针
 * @param Size 数据目标长度
 * @retval None
 */
static void Start_DMA_Tx(uint8_t *pData, uint16_t Size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID, (uint32_t)pData);
    DL_DMA_setDestAddr(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID, (uint32_t)(&UART_BLUETEETH_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID, Size);
    DL_DMA_enableChannel(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID);
}

/**
 * @brief 蓝牙模块环境初始化设置
 * @param huart 蓝牙使用的串口底层句柄指针
 * @retval None
 */
void Blueteeth_Init(UART_Regs *huart)
{
    if (huart == NULL) return;
    
    DL_UART_Main_setRXInterruptTimeout(UART_BLUETEETH_INST, 15);
    NVIC_EnableIRQ(UART_BLUETEETH_INST_INT_IRQN);

    blueteethInfo.huart = huart;
    blueteethInfo.rx_read_pos = 0;
    blueteethInfo.rx_write_pos = 0;
    blueteethInfo.tx_read_pos = 0;
    blueteethInfo.tx_write_pos = 0;
    blueteethInfo.is_tx_busy = 0;
    blueteethInfo.rxState = STATE_WAIT_HEADER;
    blueteethInfo.frameIndex = 0;

    Start_DMA_Rx(blueteethInfo.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
}

/**
 * @brief 检测并将用户发送缓冲池放入 DMA 发送
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

    Start_DMA_Tx(blueteethInfo.dma_tx_buffer, sendLen);
}

/**
 * @brief 使用格式化和 DMA 进行异步零阻塞打印
 * @param format 标准 printf 格式化控制通配符字符串
 * @param ... 可变参数，需要和打印格式对应
 * @retval None
 */
void Blueteeth_Printf_DMA(const char *format, ...)
{   
    char tempBuffer[128];
    va_list args;
    
    va_start(args, format);
    uint16_t length = vsnprintf(tempBuffer, sizeof(tempBuffer), format, args);
    va_end(args);

    if (length == 0) return;

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
 * @brief 在蓝牙助手的特定屏显坐标打印信息
 * @param x 屏幕横坐标位置
 * @param y 屏幕纵坐标位置
 * @param format 格式化字符串
 * @param ... 可变参数变量
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
 * @brief 通知蓝牙测试端清空屏幕上的数据显示
 * @param None
 * @retval None
 */
void Blueteeth_Clear(void)
{
    Blueteeth_Printf_DMA("[display-clear]\r\n");
}

/**
 * @brief 将数值信息送入波形绘制器测试
 * @param format 格式化字符串数据
 * @param ... 可变参数，数据信息
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
 * @brief 在发送完成中断内调用的处理句柄，接管后续发送任务
 * @param huart 触发中断对应的底层句柄
 * @retval None
 */
void Blueteeth_TxCpltCallback(UART_Regs *huart)
{
    if (huart == blueteethInfo.huart)
    {
        Blueteeth_DataTransmit();
    }
}

/**
 * @brief 接收结束回调函数，处理 DMA 空闲截断或完整传送事件
 * @param huart 对应的触发底层句柄
 * @param Size 实际接收的有效长度大小
 * @retval None
 */
void Blueteeth_RxEventCallback(UART_Regs *huart, uint16_t Size)
{
    if (huart != blueteethInfo.huart) return;

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
    Start_DMA_Rx(blueteethInfo.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
}

/**
 * @brief 蓝牙串口错误回调
 * @param huart 串口句柄指针
 * @retval None
 */
void Blueteeth_ErrorCallback(UART_Regs *huart)
{
    if (huart != blueteethInfo.huart) return;

    blueteethInfo.rxState = STATE_WAIT_HEADER;
    blueteethInfo.frameIndex = 0;
}

/**
 * @brief 在主循环中执行的异步蓝牙数据帧消费机任务
 * @param None
 * @retval None
 */
void Blueteeth_Task(void)
{
    /* 软件IDLE：DMA余量不变则flush */
    if (blueteeth_checkIdleFlag) {
        blueteeth_checkIdleFlag = 0;
        static uint16_t last_remain = BLUETEETH_DMA_RX_BUF_SIZE;
        uint16_t remain = DL_DMA_getTransferSize(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
        if (remain < BLUETEETH_DMA_RX_BUF_SIZE) {
            if (remain == last_remain) {
                uint16_t rxLen = BLUETEETH_DMA_RX_BUF_SIZE - remain;
                Blueteeth_RxEventCallback(UART_BLUETEETH_INST, rxLen);
            }
            last_remain = remain;
        } else {
            last_remain = BLUETEETH_DMA_RX_BUF_SIZE;
        }
    }

    while (blueteethInfo.rx_read_pos != blueteethInfo.rx_write_pos)
    {
        uint8_t dataReadingPos = blueteethInfo.rxFifo[blueteethInfo.rx_read_pos];
        blueteethInfo.rx_read_pos = (blueteethInfo.rx_read_pos + 1) % BLUETEETH_RX_FIFO_SIZE;

        switch (blueteethInfo.rxState)
        {
            case STATE_WAIT_HEADER:
                if (dataReadingPos == FRAME_HEADER) {
                    blueteethInfo.frameIndex = 0;
                    blueteethInfo.rxState = STATE_RECEIVING_DATA;
                }
                break;
            case STATE_RECEIVING_DATA:
                if (dataReadingPos == FRAME_TAIL) {
                    if (blueteethInfo.frameIndex < BLUETEETH_MAX_FRAME_LEN) {
                        blueteethInfo.frame_buffer[blueteethInfo.frameIndex] = '\0';
                    }
                    Process_Bluetooth_Command((char *)blueteethInfo.frame_buffer);
                    blueteethInfo.rxState = STATE_WAIT_HEADER;
                }
                else if (dataReadingPos == FRAME_HEADER) {
                    blueteethInfo.frameIndex = 0;
                }
                else {
                    if (blueteethInfo.frameIndex < BLUETEETH_MAX_FRAME_LEN - 1) {
                        blueteethInfo.frame_buffer[blueteethInfo.frameIndex++] = dataReadingPos;
                    }
                    else {
                        blueteethInfo.rxState = STATE_WAIT_HEADER;
                    }
                }
                break;
        }
    }
}
