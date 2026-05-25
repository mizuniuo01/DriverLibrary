/**
 * @file    cam.c
 * @brief   摄像头串口通信模块（DMA + 环形缓冲区 + 帧解析）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    依赖：UART + DMA 外设已在 SysConfig 中配置
 * @note    TI MSPM0 无硬件 IDLE 中断，需在 task 中做软件 IDLE 检测
 * @note    帧协议：0xFF 帧头 + 数据 + 0xFE 帧尾
 * @warning ISR 回调中只做数据搬运，复杂逻辑在 cam_task 中处理
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * UART DMA + 环形缓冲区 + 帧解析，与 blueteeth 架构一致。
 * 当前为单实例设计（模块内部 static handle），依赖 SysConfig 宏。
 *
 * ── 初始化 ──
 *
 * cam_init(UART_CAM_INST);
 *
 * ── ISR 配置（用户项目）──
 *
 * // UART 中断中调用 cam_rx_callback / cam_error_callback
 * // 定时器 ISR 中周期性置位 cam_check_idle_flag（软件 IDLE）
 *
 * ── 主循环 ──
 *
 * cam_task();  // 帧解析
 *
 * ── 数据读取 ──
 *
 * cam_data_t data = cam_get_data();
 */

#include "cam.h"

#include <string.h>

static cam_handle_t cam_inst;
static cam_data_t cam_data;

/**
 * @brief  启动 DMA 接收
 * @param  buf   接收缓冲区指针
 * @param  size  接收数据长度
 * @retval 无
 */
static void start_dma_rx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CAM_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA,
                      DMA_CAM_RX_CHAN_ID,
                      (uint32_t)(&UART_CAM_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CAM_RX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, DMA_CAM_RX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CAM_RX_CHAN_ID);
}

/**
 * @brief  摄像头模块初始化
 * @param  huart  绑定的串口句柄指针
 * @retval 无
 */
void cam_init(UART_Regs *huart)
{
    if (!huart) {
        return;
    }

    DL_UART_Main_setRXInterruptTimeout(UART_CAM_INST, 10);
    NVIC_EnableIRQ(UART_CAM_INST_INT_IRQN);

    cam_inst.huart = huart;
    cam_inst.rx_read_pos = 0;
    cam_inst.rx_write_pos = 0;
    cam_inst.rx_state = CAM_STATE_WAIT_HEADER;
    cam_inst.frame_index = 0;

    memset(cam_inst.dma_rx_buffer, 0, CAM_DMA_RX_BUF_SIZE);
    start_dma_rx(cam_inst.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
}

/**
 * @brief  DMA 接收完成回调（ISR 中调用），将数据推入 RX FIFO
 * @param  huart  触发中断的串口句柄
 * @param  size   实际接收数据长度
 * @retval 无
 */
void cam_rx_callback(UART_Regs *huart, uint16_t size)
{
    uint16_t i;

    if (!huart || !cam_inst.huart) {
        return;
    }

    if (huart != cam_inst.huart) {
        return;
    }

    if (size > 0) {
        for (i = 0; i < size; i++) {
            uint16_t next;

            next = (cam_inst.rx_write_pos + 1) % CAM_RX_FIFO_SIZE;
            if (next != cam_inst.rx_read_pos) {
                cam_inst.rx_fifo[cam_inst.rx_write_pos] =
                    cam_inst.dma_rx_buffer[i];
                cam_inst.rx_write_pos = next;
            }
        }
    }

    memset(cam_inst.dma_rx_buffer, 0, CAM_DMA_RX_BUF_SIZE);
    start_dma_rx(cam_inst.dma_rx_buffer, CAM_DMA_RX_BUF_SIZE);
}

/**
 * @brief  串口错误回调（ISR 中调用），复位接收状态机
 * @param  huart  触发中断的串口句柄
 * @retval 无
 */
void cam_error_callback(UART_Regs *huart)
{
    if (huart != cam_inst.huart) {
        return;
    }

    cam_inst.rx_state = CAM_STATE_WAIT_HEADER;
    cam_inst.frame_index = 0;
}

/**
 * @brief  摄像头周期性任务（主循环中调用）
 * @note   帧解析状态机：帧头 0xFF → 数据 → 帧尾 0xFE
 * @param  无
 * @retval 无
 */
void cam_task(void)
{
    uint8_t byte;

    while (cam_inst.rx_read_pos != cam_inst.rx_write_pos) {
        byte = cam_inst.rx_fifo[cam_inst.rx_read_pos];
        cam_inst.rx_read_pos =
            (cam_inst.rx_read_pos + 1) % CAM_RX_FIFO_SIZE;

        switch (cam_inst.rx_state) {
            case CAM_STATE_WAIT_HEADER:
                if (byte == CAM_FRAME_HEADER) {
                    cam_inst.frame_index = 0;
                    cam_inst.rx_state = CAM_STATE_RECEIVING_DATA;
                }
                break;

            case CAM_STATE_RECEIVING_DATA:
                if (byte == CAM_FRAME_TAIL) {
                    if (cam_inst.frame_index < CAM_MAX_FRAME_LEN) {
                        cam_inst
                            .frame_buffer[cam_inst.frame_index] = '\0';
                    }

                    /* 数据解析：根据实际数据格式填充 cam_data */
                    /* 解析逻辑由具体项目补充 */

                    cam_inst.rx_state = CAM_STATE_WAIT_HEADER;
                } else if (byte == CAM_FRAME_HEADER) {
                    cam_inst.frame_index = 0;
                } else {
                    if (cam_inst.frame_index < CAM_MAX_FRAME_LEN) {
                        cam_inst
                            .frame_buffer[cam_inst.frame_index++] = byte;
                    } else {
                        cam_inst.rx_state = CAM_STATE_WAIT_HEADER;
                    }
                }
                break;

            default:
                cam_inst.rx_state = CAM_STATE_WAIT_HEADER;
                break;
        }
    }
}

/**
 * @brief  获取摄像头解析数据
 * @param  无
 * @retval 摄像头数据结构体
 */
cam_data_t cam_get_data(void)
{
    return cam_data;
}
