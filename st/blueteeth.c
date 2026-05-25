/**
 * @file    blueteeth.c
 * @brief   蓝牙串口透传模块实现
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 2.0.0
 * @note    使用江协科技蓝牙小程序，打印和绘图函数适配小程序协议
 * @note    依赖：UART + DMA 外设已初始化，HAL 库已配置
 * @note    STM32 使用硬件 IDLE 中断检测帧结束，无需软件轮询
 * @note    若使用非 F4 系列 MCU，将头文件中的 <stm32f4xx_hal.h> 替换为对应系列
 * @warning ISR 回调中只做数据搬运，复杂逻辑在 blueteeth_task 中处理
 */

#include "blueteeth.h"

#include <stdio.h>
#include <string.h>

static BlueteethHandle_t blueteeth_inst;

/* 蓝牙指令字典，按实际需求添加条目 */
static const BlueteethCommandMap_t cmd_table[] = {
    /* {"指令", 回调函数} */
};

/**
 * @brief  在指令字典中查找匹配的命令并执行回调
 * @param  cmd_string  收到的指令字符串
 * @retval 无
 */
static void process_command(const char *cmd_string)
{
    uint8_t i;
    for (i = 0; i < CMD_TABLE_SIZE; i++) {
        if (strcmp(cmd_string, cmd_table[i].cmd_string) == 0) {
            cmd_table[i].cmd_handler();
            return;
        }
    }
}

/**
 * @brief  蓝牙模块初始化
 * @param  huart  绑定的串口句柄指针
 * @retval 无
 */
void blueteeth_init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }

    blueteeth_inst.huart = huart;
    blueteeth_inst.rx_read_pos = 0;
    blueteeth_inst.rx_write_pos = 0;
    blueteeth_inst.tx_read_pos = 0;
    blueteeth_inst.tx_write_pos = 0;
    blueteeth_inst.is_tx_busy = 0;
    blueteeth_inst.rx_state = STATE_WAIT_HEADER;
    blueteeth_inst.frame_index = 0;

    HAL_UARTEx_ReceiveToIdle_DMA(blueteeth_inst.huart,
                                 blueteeth_inst.dma_rx_buffer,
                                 BLUETEETH_DMA_RX_BUF_SIZE);
}

/**
 * @brief  从发送 FIFO 搬运数据到 DMA 发送缓冲区并启动发送
 * @param  无
 * @retval 无
 */
static void data_transmit(void)
{
    uint16_t send_len;
    uint16_t len_to_copy;

    if (blueteeth_inst.tx_write_pos == blueteeth_inst.tx_read_pos) {
        blueteeth_inst.is_tx_busy = 0;
        return;
    }

    blueteeth_inst.is_tx_busy = 1;

    if (blueteeth_inst.tx_write_pos > blueteeth_inst.tx_read_pos) {
        send_len = blueteeth_inst.tx_write_pos - blueteeth_inst.tx_read_pos;
        len_to_copy = (send_len > sizeof(blueteeth_inst.dma_tx_buffer))
                          ? sizeof(blueteeth_inst.dma_tx_buffer)
                          : send_len;
        memcpy(blueteeth_inst.dma_tx_buffer,
               &blueteeth_inst.tx_fifo[blueteeth_inst.tx_read_pos],
               len_to_copy);
        blueteeth_inst.tx_read_pos += len_to_copy;
        send_len = len_to_copy;
    } else {
        send_len = BLUETEETH_TX_FIFO_SIZE - blueteeth_inst.tx_read_pos;
        len_to_copy = (send_len > sizeof(blueteeth_inst.dma_tx_buffer))
                          ? sizeof(blueteeth_inst.dma_tx_buffer)
                          : send_len;
        memcpy(blueteeth_inst.dma_tx_buffer,
               &blueteeth_inst.tx_fifo[blueteeth_inst.tx_read_pos],
               len_to_copy);

        blueteeth_inst.tx_read_pos += len_to_copy;
        if (blueteeth_inst.tx_read_pos >= BLUETEETH_TX_FIFO_SIZE) {
            blueteeth_inst.tx_read_pos = 0;
        }
        send_len = len_to_copy;
    }

    HAL_UART_Transmit_DMA(
        blueteeth_inst.huart, blueteeth_inst.dma_tx_buffer, send_len);
}

/**
 * @brief  通过蓝牙发送格式化字符串（DMA + FIFO，非阻塞）
 * @param  format  格式化字符串
 * @param  ...     可变参数
 * @retval 无
 */
void blueteeth_printf(const char *format, ...)
{
    char temp_buf[128];
    va_list args;
    int ret;
    uint16_t length;
    uint32_t primask;
    uint16_t i;

    va_start(args, format);
    ret = vsnprintf(temp_buf, sizeof(temp_buf), format, args);
    va_end(args);

    if (ret <= 0) {
        return;
    }
    length = (uint16_t)ret;

    primask = __get_PRIMASK();
    __disable_irq();

    for (i = 0; i < length; i++) {
        uint16_t next;

        next = (blueteeth_inst.tx_write_pos + 1) % BLUETEETH_TX_FIFO_SIZE;
        if (next != blueteeth_inst.tx_read_pos) {
            blueteeth_inst.tx_fifo[blueteeth_inst.tx_write_pos] = temp_buf[i];
            blueteeth_inst.tx_write_pos = next;
        }
    }

    __set_PRIMASK(primask);

    if (blueteeth_inst.is_tx_busy == 0) {
        data_transmit();
    }
}

/**
 * @brief  在蓝牙调试助手指定坐标显示信息
 * @param  x       X 坐标
 * @param  y       Y 坐标
 * @param  format  格式化字符串
 * @param  ...     可变参数
 * @retval 无
 */
void blueteeth_display(int x, int y, const char *format, ...)
{
    char temp_buf[128];
    va_list args;

    va_start(args, format);
    vsnprintf(temp_buf, sizeof(temp_buf), format, args);
    va_end(args);

    blueteeth_printf("[display,%d,%d,%s]\r\n", x, y, temp_buf);
}

/**
 * @brief  通知蓝牙调试助手清屏
 * @param  无
 * @retval 无
 */
void blueteeth_clear(void)
{
    blueteeth_printf("[display-clear]\r\n");
}

/**
 * @brief  向蓝牙调试助手波形绘制器发送数据
 * @param  format  格式化字符串
 * @param  ...     可变参数
 * @retval 无
 */
void blueteeth_plot(const char *format, ...)
{
    char temp_buf[128];
    va_list args;

    va_start(args, format);
    vsnprintf(temp_buf, sizeof(temp_buf), format, args);
    va_end(args);

    blueteeth_printf("[plot,%s]\r\n", temp_buf);
}

/**
 * @brief  DMA 发送完成回调（ISR 中调用），启动下一轮发送
 * @param  huart  触发中断的串口句柄
 * @retval 无
 */
void blueteeth_tx_callback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || blueteeth_inst.huart == NULL) {
        return;
    }

    if (huart->Instance == blueteeth_inst.huart->Instance) {
        data_transmit();
    }
}

/**
 * @brief  UART 接收完成回调（ISR 中调用），将 DMA 数据推入 RX FIFO
 * @param  huart  触发中断的串口句柄
 * @param  size   实际接收数据长度
 * @retval 无
 */
void blueteeth_rx_callback(UART_HandleTypeDef *huart, uint16_t size)
{
    uint16_t i;

    if (huart == NULL || blueteeth_inst.huart == NULL) {
        return;
    }

    if (huart->Instance == blueteeth_inst.huart->Instance) {
        for (i = 0; i < size; i++) {
            uint16_t next;

            next = (blueteeth_inst.rx_write_pos + 1) % BLUETEETH_RX_FIFO_SIZE;
            if (next != blueteeth_inst.rx_read_pos) {
                blueteeth_inst.rx_fifo[blueteeth_inst.rx_write_pos] =
                    blueteeth_inst.dma_rx_buffer[i];
                blueteeth_inst.rx_write_pos = next;
            }
        }

        memset(blueteeth_inst.dma_rx_buffer, 0, BLUETEETH_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(blueteeth_inst.huart,
                                     blueteeth_inst.dma_rx_buffer,
                                     BLUETEETH_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief  蓝牙模块周期性任务（主循环中调用）
 * @note   STM32 硬件 IDLE 中断驱动数据到达，task 仅做帧解析
 * @param  无
 * @retval 无
 */
void blueteeth_task(void)
{
    uint8_t byte;

    while (blueteeth_inst.rx_read_pos != blueteeth_inst.rx_write_pos) {
        byte = blueteeth_inst.rx_fifo[blueteeth_inst.rx_read_pos];
        blueteeth_inst.rx_read_pos =
            (blueteeth_inst.rx_read_pos + 1) % BLUETEETH_RX_FIFO_SIZE;

        switch (blueteeth_inst.rx_state) {
            case STATE_WAIT_HEADER:
                if (byte == FRAME_HEADER) {
                    blueteeth_inst.frame_index = 0;
                    blueteeth_inst.rx_state = STATE_RECEIVING_DATA;
                }
                break;

            case STATE_RECEIVING_DATA:
                if (byte == FRAME_TAIL) {
                    if (blueteeth_inst.frame_index < BLUETEETH_MAX_FRAME_LEN) {
                        blueteeth_inst
                            .frame_buffer[blueteeth_inst.frame_index] = '\0';
                    }
                    process_command((char *)blueteeth_inst.frame_buffer);
                    blueteeth_inst.rx_state = STATE_WAIT_HEADER;
                } else if (byte == FRAME_HEADER) {
                    /* 帧内出现帧头，重置接收 */
                    blueteeth_inst.frame_index = 0;
                } else {
                    if (blueteeth_inst.frame_index <
                        BLUETEETH_MAX_FRAME_LEN - 1) {
                        blueteeth_inst
                            .frame_buffer[blueteeth_inst.frame_index++] = byte;
                    } else {
                        /* 帧超长，丢弃 */
                        blueteeth_inst.rx_state = STATE_WAIT_HEADER;
                    }
                }
                break;
        }
    }
}
