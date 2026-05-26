/**
 * @file    sensor.c
 * @brief   感为科技八路灰度传感器驱动（I2C 轮询状态机）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    仅适配带 MCU 的感为科技八路灰度传感器
 * @note    I2C 协议，7 位地址 0x4C，发送 0xDD 命令读取数字量
 * @note    数据位序：低电平(0)=黑线，高电平(1)=白底，模块内部取反
 * @note    sensor_request 为 I2C 轮询状态机，由定时器 ISR 周期性调用
 * @warning I2C 通信期间不可重入，通过 pol_state 状态机保证
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * I2C 轮询状态机：IDLE → SEND_CMD → WAIT_READ → 取反处理。
 *
 * ── 初始化 ──
 *
 * sensor_init(I2C_SENSOR_INST);
 *
 * ── ISR 中周期性调用 ──
 *
 * sensor_request();  // 驱动 I2C 状态机
 *
 * ── 数据读取 ──
 *
 * uint8_t data = sensor_read_data();
 * // bit0=最左侧探头 ... bit7=最右侧探头，1=黑线
 */

#include "sensor.h"

/* I2C 轮询状态 */
typedef enum {
    MODE_IDLE = 0,
    MODE_SEND_CMD,
    MODE_WAIT_READ,
} sensor_pol_state_t;

static I2C_Regs *sensor_hi2c;
static uint8_t sensor_rx_buffer[1];
static volatile uint8_t sensor_processed_data;
static volatile sensor_pol_state_t pol_state = MODE_IDLE;

/**
 * @brief  传感器 I2C 外设初始化
 * @param  hi2c  I2C 外设句柄
 * @retval DRV_OK 成功，DRV_ERR_PARAM 参数非法
 */
drv_err_t sensor_init(I2C_Regs *hi2c)
{
    if (!hi2c) {
        return DRV_ERR_PARAM;
    }

    NVIC_EnableIRQ(I2C_SENSOR_INST_INT_IRQN);
    DL_I2C_setTimeoutACount(I2C_SENSOR_INST, 0xFFFFF);
    DL_I2C_enableTimeoutA(I2C_SENSOR_INST);
    DL_I2C_enableInterrupt(I2C_SENSOR_INST,
                           DL_I2C_INTERRUPT_TIMEOUT_A | DL_I2C_INTERRUPT_CONTROLLER_NACK |
                               DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
    NVIC_SetPriority(I2C_SENSOR_INST_INT_IRQN, 3);

    sensor_hi2c = hi2c;
    pol_state = MODE_IDLE;

    return DRV_OK;
}

/**
 * @brief  获取传感器忙闲状态
 * @param  无
 * @retval 当前状态
 */
sensor_state_t sensor_get_state(void)
{
    return (pol_state == MODE_IDLE) ? SENSOR_STATE_IDLE : SENSOR_STATE_BUSY;
}

/**
 * @brief  I2C 轮询状态机（由定时器 ISR 周期性调用）
 * @note   非阻塞：每次调用只推进一步，不等待
 * @param  无
 * @retval 无
 */
void sensor_request(void)
{
    if (!sensor_hi2c) {
        return;
    }

    if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_ERROR) {
        sensor_error_callback(sensor_hi2c);
        return;
    }

    switch (pol_state) {
        case MODE_IDLE:
            if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_IDLE) {
                DL_I2C_clearInterruptStatus(sensor_hi2c, 0xFFFFFFFF);

                DL_I2C_fillControllerTXFIFO(
                    sensor_hi2c, &((uint8_t){SENSOR_CMD_READ_DIG}), 1);

                DL_I2C_startControllerTransfer(
                    sensor_hi2c, SENSOR_I2C_ADDR_7BIT, DL_I2C_CONTROLLER_DIRECTION_TX, 1);

                pol_state = MODE_SEND_CMD;
            }
            break;

        case MODE_SEND_CMD:
            if (DL_I2C_getControllerStatus(sensor_hi2c) &
                DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
                if (DL_I2C_getControllerStatus(sensor_hi2c) &
                    DL_I2C_CONTROLLER_STATUS_ERROR) {
                    sensor_error_callback(sensor_hi2c);
                }
                return;
            }

            DL_I2C_startControllerTransfer(
                sensor_hi2c, SENSOR_I2C_ADDR_7BIT, DL_I2C_CONTROLLER_DIRECTION_RX, 1);
            pol_state = MODE_WAIT_READ;
            break;

        case MODE_WAIT_READ:
            if (DL_I2C_isControllerRXFIFOEmpty(sensor_hi2c)) {
                if (DL_I2C_getControllerStatus(sensor_hi2c) &
                    DL_I2C_CONTROLLER_STATUS_ERROR) {
                    sensor_error_callback(sensor_hi2c);
                }
                return;
            }

            sensor_rx_buffer[0] = DL_I2C_receiveControllerData(sensor_hi2c);

            {
                uint8_t raw_data = sensor_rx_buffer[0];
                uint8_t temp_data = 0;
                uint8_t i;

                /*
                 * 位序标准：bit0=最左探头, bit7=最右探头
                 * 原始数据低电平(0)=黑线，取反转为正逻辑(1=黑线)
                 */
                for (i = 0; i < 8; i++) {
                    if (((raw_data >> i) & 0x01) == 0) {
                        temp_data |= (1 << i);
                    }
                }

                sensor_processed_data = temp_data;
            }

            pol_state = MODE_IDLE;
            break;

        default:
            pol_state = MODE_IDLE;
            break;
    }
}

/**
 * @brief  I2C 接收完成回调（ISR 中调用）
 * @param  hi2c  I2C 句柄指针
 * @retval 无
 */
void sensor_rx_callback(I2C_Regs *hi2c)
{
    if (!hi2c || hi2c != sensor_hi2c) {
        return;
    }

    pol_state = MODE_IDLE;
}

/**
 * @brief  I2C 错误回调（ISR 中调用）
 * @param  hi2c  I2C 句柄指针
 * @retval 无
 */
void sensor_error_callback(I2C_Regs *hi2c)
{
    if (!hi2c || hi2c != sensor_hi2c) {
        return;
    }

    DL_I2C_resetControllerTransfer(hi2c);
    DL_I2C_clearInterruptStatus(hi2c, 0xFFFFFFFF);
    pol_state = MODE_IDLE;
}

/**
 * @brief  读取转换后的传感器数据
 * @param  无
 * @retval 8 位灰度数据（bit0=最左, 1=黑线）
 */
uint8_t sensor_read_data(void)
{
    return sensor_processed_data;
}
