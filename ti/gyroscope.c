/**
 * @file    gyroscope.c
 * @brief   姿态传感器串口通信模块（DMA + 环形缓冲区 + 帧解析）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：UART + DMA 外设已在 SysConfig 中配置
 * @note    TI MSPM0 无硬件 IDLE 中断，使用 DMA 余量不变判定法
 * @note    帧协议：0x55 + 0x53 + 8字节数据 + 校验和（共11字节）
 * @warning ISR 回调中只做数据搬运，浮点转换在 gyro_task 中完成
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * UART DMA + 环形缓冲区 + 三态帧解析（头→类型→数据）。
 * 当前为单实例设计，依赖 SysConfig 宏。
 *
 * ── 初始化 ──
 *
 * gyro_init(UART_GYROSCOPE_INST);
 *
 * ── ISR 配置（用户项目）──
 *
 * // UART 中断中调用 gyro_rx_callback / gyro_error_callback
 * // 定时器 ISR 中每 2ms 置 gyro_check_idle_flag = 1
 *
 * ── 主循环 ──
 *
 * gyro_task();  // 软件 IDLE 检测 + 帧解析
 *
 * ── 数据读取 ──
 *
 * gyro_data_t data = gyro_get_data();
 * // data.roll, data.pitch, data.yaw（度）
 */

#include "gyroscope.h"
#include <string.h>

static gyro_handle_t gyro_inst;
static gyro_data_t gyro_data;

volatile uint8_t gyro_check_idle_flag;

/**
 * @brief  启动 DMA 接收
 * @param  buf   接收缓冲区指针
 * @param  size  接收数据长度
 * @retval 无
 */
static void start_dma_rx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
    DL_DMA_setSrcAddr(
        DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, (uint32_t)(&UART_GYROSCOPE_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
}

/**
 * @brief  姿态传感器模块初始化
 * @param  huart  绑定的串口句柄指针
 * @retval 无
 */
void gyro_init(UART_Regs *huart)
{
    if (!huart) {
        return;
    }

    DL_UART_Main_setRXInterruptTimeout(UART_GYROSCOPE_INST, 10);
    NVIC_EnableIRQ(UART_GYROSCOPE_INST_INT_IRQN);

    gyro_inst.huart = huart;
    gyro_inst.rx_read_pos = 0;
    gyro_inst.rx_write_pos = 0;
    gyro_inst.rx_state = GYRO_STATE_WAIT_HEADER;
    gyro_inst.frame_index = 0;

    start_dma_rx(gyro_inst.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
}

/**
 * @brief  DMA 接收完成回调（ISR 中调用），将数据推入 RX FIFO
 * @param  huart  触发中断的串口句柄
 * @param  size   实际接收数据长度
 * @retval 无
 */
void gyro_rx_callback(UART_Regs *huart, uint16_t size)
{
    uint16_t i;

    if (!huart || !gyro_inst.huart) {
        return;
    }

    if (huart != gyro_inst.huart) {
        return;
    }

    if (size > 0) {
        for (i = 0; i < size; i++) {
            uint16_t next;

            next = (gyro_inst.rx_write_pos + 1) % GYRO_RX_FIFO_SIZE;
            if (next != gyro_inst.rx_read_pos) {
                gyro_inst.rx_fifo[gyro_inst.rx_write_pos] = gyro_inst.dma_rx_buffer[i];
                gyro_inst.rx_write_pos = next;
            }
        }
    }

    memset(gyro_inst.dma_rx_buffer, 0, GYRO_DMA_RX_BUF_SIZE);
    start_dma_rx(gyro_inst.dma_rx_buffer, GYRO_DMA_RX_BUF_SIZE);
}

/**
 * @brief  串口错误回调（ISR 中调用），复位接收状态机
 * @param  huart  触发中断的串口句柄
 * @retval 无
 */
void gyro_error_callback(UART_Regs *huart)
{
    if (huart != gyro_inst.huart) {
        return;
    }

    gyro_inst.rx_state = GYRO_STATE_WAIT_HEADER;
    gyro_inst.frame_index = 0;
}

/**
 * @brief  校验和验证
 * @param  buf  数据缓冲区
 * @param  len  数据长度（含校验字节）
 * @retval 0=校验失败，1=校验通过
 */
static uint8_t verify_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t sum;

    sum = 0;
    for (i = 0; i < len - 1; i++) {
        sum += buf[i];
    }

    return (sum == buf[len - 1]) ? 1 : 0;
}

/**
 * @brief  解析角度数据帧并转换为度
 * @param  buf  11 字节原始数据帧
 * @retval 无
 */
static void parse_angle_frame(const uint8_t *buf)
{
    int16_t roll_raw;
    int16_t pitch_raw;
    int16_t yaw_raw;

    roll_raw = (int16_t)((buf[3] << 8) | buf[2]);
    pitch_raw = (int16_t)((buf[5] << 8) | buf[4]);
    yaw_raw = (int16_t)((buf[7] << 8) | buf[6]);

    gyro_data.roll = (float)roll_raw / 32768.0f * 180.0f;
    gyro_data.pitch = (float)pitch_raw / 32768.0f * 180.0f;
    gyro_data.yaw = (float)yaw_raw / 32768.0f * 180.0f;
}

/**
 * @brief  姿态传感器周期性任务（主循环中调用）
 * @note   软件 IDLE 检测 + 三态帧解析
 * @note   帧结构：0x55 → 0x53 → 8字节数据+校验 → 校验通过后解析
 * @param  无
 * @retval 无
 */
void gyro_task(void)
{
    uint8_t byte;

    /* 软件 IDLE：DMA 余量连续两次不变则判定帧结束 */
    if (gyro_check_idle_flag) {
        static uint16_t last_remain = GYRO_DMA_RX_BUF_SIZE;
        uint16_t remain;

        gyro_check_idle_flag = 0;

        remain = DL_DMA_getTransferSize(DMA, DMA_CH_GYROSCOPE_RX_CHAN_ID);
        if (remain < GYRO_DMA_RX_BUF_SIZE) {
            if (remain == last_remain) {
                uint16_t rx_len;

                rx_len = GYRO_DMA_RX_BUF_SIZE - remain;
                gyro_rx_callback(UART_GYROSCOPE_INST, rx_len);
            }
            last_remain = remain;
        } else {
            last_remain = GYRO_DMA_RX_BUF_SIZE;
        }
    }

    /* 三态帧解析 */
    while (gyro_inst.rx_read_pos != gyro_inst.rx_write_pos) {
        byte = gyro_inst.rx_fifo[gyro_inst.rx_read_pos];
        gyro_inst.rx_read_pos = (gyro_inst.rx_read_pos + 1) % GYRO_RX_FIFO_SIZE;

        switch (gyro_inst.rx_state) {
            case GYRO_STATE_WAIT_HEADER:
                if (byte == GYRO_FRAME_HEADER) {
                    gyro_inst.frame_buffer[0] = byte;
                    gyro_inst.frame_index = 1;
                    gyro_inst.rx_state = GYRO_STATE_WAIT_TYPE;
                }
                break;

            case GYRO_STATE_WAIT_TYPE:
                if (byte == GYRO_TYPE_ANGLE) {
                    gyro_inst.frame_buffer[1] = byte;
                    gyro_inst.frame_index = 2;
                    gyro_inst.rx_state = GYRO_STATE_RECEIVING_DATA;
                } else if (byte == GYRO_FRAME_HEADER) {
                    gyro_inst.frame_buffer[0] = byte;
                    gyro_inst.frame_index = 1;
                } else {
                    gyro_inst.rx_state = GYRO_STATE_WAIT_HEADER;
                }
                break;

            case GYRO_STATE_RECEIVING_DATA:
                gyro_inst.frame_buffer[gyro_inst.frame_index++] = byte;

                if (gyro_inst.frame_index >= GYRO_FRAME_LEN) {
                    if (verify_checksum(gyro_inst.frame_buffer, GYRO_FRAME_LEN)) {
                        parse_angle_frame(gyro_inst.frame_buffer);
                    }

                    gyro_inst.rx_state = GYRO_STATE_WAIT_HEADER;
                }
                break;

            default:
                gyro_inst.rx_state = GYRO_STATE_WAIT_HEADER;
                break;
        }
    }
}

/**
 * @brief  获取姿态传感器数据
 * @param  无
 * @retval 姿态角数据结构体（roll/pitch/yaw，度）
 */
gyro_data_t gyro_get_data(void)
{
    return gyro_data;
}
