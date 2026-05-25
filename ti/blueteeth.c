/**
 * @file    blueteeth.c
 * @brief   蓝牙串口透传模块（DMA + 环形缓冲区 + 帧解析状态机）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 2.0.0
 * @note    使用江协科技蓝牙小程序，打印和绘图函数适配小程序协议
 * @note    依赖：UART + DMA 外设已在 SysConfig 中配置并生成 ti_msp_dl_config.h
 * @note    TI MSPM0 无硬件 IDLE 中断，使用 DMA 余量不变判定法检测帧结束
 * @warning ISR 回调中只做数据搬运，复杂逻辑在 blueteeth_task 中处理
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 通信层模块，提供蓝牙串口透传 + 格式化输出 + 指令接收解析。
 * 当前为单实例设计（模块内部 static handle），严重依赖
 * SysConfig 生成的宏（见下文"硬依赖清单"），因此无法像
 * buzzer/led/key 那样通过 cfg 结构体完全解耦硬件。
 *
 * ── 初始化 ──
 *
 * void system_init(void)
 * {
 *     blueteeth_init(UART_BLUETEETH_INST);
 * }
 *
 * ── ISR 配置 ──
 *
 * // 1. UART 中断：调用 blueteeth 回调
 * void UART_BLUETEETH_INST_IRQHandler(void)
 * {
 *     // 用户自行判断中断源，调用对应回调：
 *     // - TX DMA 完成 → blueteeth_tx_callback(UART_BLUETEETH_INST)
 *     // - RX 完成/超时 → blueteeth_rx_callback(UART_BLUETEETH_INST, size)
 *     // - 错误       → blueteeth_error_callback(UART_BLUETEETH_INST)
 * }
 *
 * // 2. 定时器 ISR：周期性置位软件 IDLE 检测标志（TI 平台特有）
 * //    周期建议 2ms，STM32 平台不需要此步骤
 * void Timer_IRQHandler(void)
 * {
 *     blueteeth_check_idle_flag = 1;
 * }
 *
 * ── 主循环 ──
 *
 * void main(void)
 * {
 *     system_init();
 *     while (1) {
 *         blueteeth_task();  // 帧解析 + 软件 IDLE 检测
 *     }
 * }
 *
 * ── 发送数据 ──
 *
 * // 格式化文本
 * blueteeth_printf("sensor: %d, speed: %d\r\n", val, spd);
 *
 * // 定点显示（在手机 APP 指定坐标显示）
 * blueteeth_display(0, 20, "speed: %d", spd);
 *
 * // 波形绘图
 * blueteeth_plot("%d,%d,%d", ch1, ch2, ch3);
 *
 * // 清屏
 * blueteeth_clear();
 *
 * ── 接收指令 ──
 *
 * // 在 blueteeth.c 的 cmd_table[] 中添加指令映射：
 * static const BlueteethCommandMap_t cmd_table[] = {
 *     {"START", on_start_cmd},    // 收到 @START# 时调用 on_start_cmd()
 *     {"STOP",  on_stop_cmd},
 * };
 *
 * ── 硬依赖清单（全部由 SysConfig 生成，不可改名） ──
 *
 * UART_BLUETEETH_INST               UART 外设基地址
 * UART_BLUETEETH_INST_INT_IRQN      UART 中断向量号
 * DMA                                DMA 外设基地址
 * DMA_CH_BLUETEETH_RX_CHAN_ID       DMA 接收通道号
 * DMA_CH_BLUETEETH_TX_CHAN_ID       DMA 发送通道号
 *
 * ── 缓冲区宏说明 ──
 *
 * BLUETEETH_DMA_RX_BUF_SIZE  128   单次 DMA 接收缓冲
 * BLUETEETH_DMA_TX_BUF_SIZE  128   单次 DMA 发送缓冲
 * BLUETEETH_RX_FIFO_SIZE     1024  接收环形队列（高吞吐时加大）
 * BLUETEETH_TX_FIFO_SIZE     1024  发送环形队列
 * BLUETEETH_MAX_FRAME_LEN    128   单帧最大长度
 * FRAME_HEADER               '@'   帧头
 * FRAME_TAIL                 '#'   帧尾
 */

#include "blueteeth.h"

#include <stdio.h>
#include <string.h>

static BlueteethHandle_t blueteeth_inst;
volatile uint8_t blueteeth_check_idle_flag;

/* 蓝牙指令字典，按实际需求添加条目 */
static const BlueteethCommandMap_t cmd_table[] = {
    /* {"指令", 回调函数} */
};

/* 指令表条目数 */
#define CMD_TABLE_SIZE (sizeof(cmd_table) / sizeof(cmd_table[0]))

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
 * @brief  启动 DMA 接收
 * @param  buf   接收缓冲区指针
 * @param  size  接收数据长度
 * @retval 无
 */
static void start_dma_rx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA,
                      DMA_CH_BLUETEETH_RX_CHAN_ID,
                      (uint32_t)(&UART_BLUETEETH_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
}

/**
 * @brief  启动 DMA 发送
 * @param  buf   发送缓冲区指针
 * @param  size  发送数据长度
 * @retval 无
 */
static void start_dma_tx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setDestAddr(DMA,
                       DMA_CH_BLUETEETH_TX_CHAN_ID,
                       (uint32_t)(&UART_BLUETEETH_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CH_BLUETEETH_TX_CHAN_ID);
}

/**
 * @brief  蓝牙模块初始化
 * @param  huart  绑定的串口句柄指针
 * @retval 无
 */
void blueteeth_init(UART_Regs *huart)
{
    if (!huart) {
        return;
    }

    DL_UART_Main_setRXInterruptTimeout(UART_BLUETEETH_INST, 15);
    NVIC_EnableIRQ(UART_BLUETEETH_INST_INT_IRQN);

    blueteeth_inst.huart = huart;
    blueteeth_inst.rx_read_pos = 0;
    blueteeth_inst.rx_write_pos = 0;
    blueteeth_inst.tx_read_pos = 0;
    blueteeth_inst.tx_write_pos = 0;
    blueteeth_inst.is_tx_busy = 0;
    blueteeth_inst.rx_state = STATE_WAIT_HEADER;
    blueteeth_inst.frame_index = 0;

    start_dma_rx(blueteeth_inst.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
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

    start_dma_tx(blueteeth_inst.dma_tx_buffer, send_len);
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
    int32_t ret;
    uint16_t length;
    uint32_t primask;
    uint16_t i;

    if (!format) {
        return;
    }

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
void blueteeth_display(int16_t x, int16_t y, const char *format, ...)
{
    char temp_buf[128];
    va_list args;

    if (!format) {
        return;
    }

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

    if (!format) {
        return;
    }

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
void blueteeth_tx_callback(UART_Regs *huart)
{
    if (huart == blueteeth_inst.huart) {
        data_transmit();
    }
}

/**
 * @brief  UART 接收完成回调（ISR 中调用），将 DMA 数据推入 RX FIFO
 * @param  huart  触发中断的串口句柄
 * @param  size   实际接收数据长度
 * @retval 无
 */
void blueteeth_rx_callback(UART_Regs *huart, uint16_t size)
{
    uint16_t i;

    if (huart != blueteeth_inst.huart) {
        return;
    }

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
    start_dma_rx(blueteeth_inst.dma_rx_buffer, BLUETEETH_DMA_RX_BUF_SIZE);
}

/**
 * @brief  串口错误回调（ISR 中调用），复位接收状态机
 * @param  huart  触发中断的串口句柄
 * @retval 无
 */
void blueteeth_error_callback(UART_Regs *huart)
{
    if (huart != blueteeth_inst.huart) {
        return;
    }

    blueteeth_inst.rx_state = STATE_WAIT_HEADER;
    blueteeth_inst.frame_index = 0;
}

/**
 * @brief  蓝牙模块周期性任务（主循环中调用）
 * @note   TI 平台特化：通过 DMA 余量不变判定法检测帧结束
 * @param  无
 * @retval 无
 */
void blueteeth_task(void)
{
    static uint16_t last_remain = BLUETEETH_DMA_RX_BUF_SIZE;
    uint16_t remain;
    uint16_t rx_len;
    uint8_t byte;

    /* 软件 IDLE 检测：DMA 余量连续两次不变则判定帧结束 */
    if (blueteeth_check_idle_flag) {
        blueteeth_check_idle_flag = 0;

        remain = DL_DMA_getTransferSize(DMA, DMA_CH_BLUETEETH_RX_CHAN_ID);
        if (remain < BLUETEETH_DMA_RX_BUF_SIZE) {
            if (remain == last_remain) {
                rx_len = BLUETEETH_DMA_RX_BUF_SIZE - remain;
                blueteeth_rx_callback(UART_BLUETEETH_INST, rx_len);
            }
            last_remain = remain;
        } else {
            last_remain = BLUETEETH_DMA_RX_BUF_SIZE;
        }
    }

    /* 帧解析 */
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

            default:
                blueteeth_inst.rx_state = STATE_WAIT_HEADER;
                break;
        }
    }
}
