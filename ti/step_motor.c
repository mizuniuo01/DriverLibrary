/**
 * @file    step_motor.c
 * @brief   双轴步进电机驱动模块（张大头 ZDT X42S 第二代，TI DriverLib）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    仅适配张大头 ZDT X42S 第二代闭环步进电机（X 固件），指令模式
 * @note    DMA + FIFO + 软件 IDLE 检测（MSPM0 无硬件 IDLE）
 * @note    协议：地址 + 命令 + 数据 + 固定校验 0x6B（手册 §4.1.1）
 * @note    含心跳检测、软限位、梯形加减速控制
 * @note    协议细节以官方用户手册 V1.0.3 为准
 * @warning 上电初始化阶段使用 delay_cycles 等待底板稳定
 * @warning 软件 IDLE 依赖 step_motor_check_idle_flag，需定时器 ISR 周期性置位
 * @note    参数非法时通过 error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM) 上报
 *
 * @usage
 * step_motor_init(UART_STEPMOTOR_INST);
 * // 定时器 ISR 中每 2ms 置 step_motor_check_idle_flag = 1
 * // UART ISR 中调用 step_motor_tx_callback / step_motor_error_callback
 * // 主循环中调用 step_motor_data_task()
 * step_motor_set_angle(MOTOR_ID_X, MOTOR_MODE_ABS, 90.0f, 800, 300, 0);
 */

#include "step_motor.h"
#include "../error_handler.h"
#include <math.h>
#include <string.h>

/* 通信上下文（模块私有） */
static step_motor_comm_t motor_comm;

static step_motor_t motor_x = {
    .id = MOTOR_ID_X,
    .target_angle = 0.0f,
    .current_angle = 0.0f,
    .is_reached = 1,
    .max_angle = 900.0f,
    .min_angle = -900.0f,
    .last_ack_time = 0,
    .is_online = 1,
    .is_error = 0,
};

static step_motor_t motor_y = {
    .id = MOTOR_ID_Y,
    .target_angle = 0.0f,
    .current_angle = 0.0f,
    .is_reached = 1,
    .max_angle = 10.0f,
    .min_angle = -10.0f,
    .last_ack_time = 0,
    .is_online = 1,
    .is_error = 0,
};

volatile uint8_t step_motor_check_idle_flag;

/* 获取系统 tick（ms），需应用层提供实现 */
extern uint32_t get_system_tick(void);

/**
 * @brief  启动 DMA 接收
 * @param  buf   接收缓冲区指针
 * @param  size  接收数据长度
 * @retval 无
 */
static void start_dma_rx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID,
        (uint32_t)(&UART_STEPMOTOR_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID);
}

/**
 * @brief  启动 DMA 发送
 * @param  buf   发送缓冲区指针
 * @param  size  发送数据长度
 * @retval 无
 */
static void start_dma_tx(uint8_t *buf, uint16_t size)
{
    DL_DMA_disableChannel(DMA, DMA_CH_STEPMOTOR_TX_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH_STEPMOTOR_TX_CHAN_ID, (uint32_t)buf);
    DL_DMA_setDestAddr(DMA, DMA_CH_STEPMOTOR_TX_CHAN_ID,
        (uint32_t)(&UART_STEPMOTOR_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH_STEPMOTOR_TX_CHAN_ID, size);
    DL_DMA_enableChannel(DMA, DMA_CH_STEPMOTOR_TX_CHAN_ID);
}

/**
 * @brief  发送 FIFO 推出到 DMA 缓冲区并启动硬件发送
 * @param  无
 * @retval 无
 */
static void data_transmit(void)
{
    uint16_t send_len;

    if (motor_comm.tx_write_pos == motor_comm.tx_read_pos) {
        motor_comm.is_tx_busy = 0;
        return;
    }

    motor_comm.is_tx_busy = 1;

    if (motor_comm.tx_write_pos > motor_comm.tx_read_pos) {
        send_len = motor_comm.tx_write_pos - motor_comm.tx_read_pos;

        if (send_len > STEPMOTOR_DMA_TX_BUF_SIZE) {
            send_len = STEPMOTOR_DMA_TX_BUF_SIZE;
        }

        memcpy(motor_comm.dma_tx_buffer, &motor_comm.tx_fifo[motor_comm.tx_read_pos],
            send_len);
        motor_comm.tx_read_pos += send_len;
    } else {
        send_len = STEPMOTOR_TX_FIFO_SIZE - motor_comm.tx_read_pos;

        if (send_len > STEPMOTOR_DMA_TX_BUF_SIZE) {
            send_len = STEPMOTOR_DMA_TX_BUF_SIZE;
        }

        memcpy(motor_comm.dma_tx_buffer, &motor_comm.tx_fifo[motor_comm.tx_read_pos],
            send_len);
        motor_comm.tx_read_pos += send_len;

        if (motor_comm.tx_read_pos >= STEPMOTOR_TX_FIFO_SIZE) {
            motor_comm.tx_read_pos = 0;
        }
    }

    start_dma_tx(motor_comm.dma_tx_buffer, send_len);
}

/**
 * @brief  写入发送 FIFO 并触发发送
 * @param  data  数据指针
 * @param  len   数据长度
 * @retval 无
 */
static void push_tx_data(uint8_t *data, uint16_t len)
{
    uint32_t primask;
    uint16_t next;
    uint16_t i;

    primask = __get_PRIMASK();
    __disable_irq();

    for (i = 0; i < len; i++) {
        next = (motor_comm.tx_write_pos + 1) % STEPMOTOR_TX_FIFO_SIZE;

        if (next != motor_comm.tx_read_pos) {
            motor_comm.tx_fifo[motor_comm.tx_write_pos] = data[i];
            motor_comm.tx_write_pos = next;
        }
    }

    __set_PRIMASK(primask);

    if (motor_comm.is_tx_busy == 0) {
        data_transmit();
    }
}

/**
 * @brief  解析应答帧并更新电机状态（手册 §4.1.2）
 * @note   读位置返回 8B：Addr+36+符号+位置(4B)+6B
 * @note   运动命令返回 4B：Addr+FD/FE/FF+状态+6B
 * @param  frame   帧数据
 * @param  length  帧长度
 * @retval 无
 */
static void process_reply(uint8_t *frame, uint8_t length)
{
    step_motor_t *motor;
    uint32_t raw_position;
    uint8_t receive_id;
    uint8_t command_val;
    uint8_t sign_flag;
    uint8_t status_val;

    receive_id = frame[0];
    command_val = frame[1];

    if (receive_id != MOTOR_ID_X && receive_id != MOTOR_ID_Y) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return;
    }
    motor = (receive_id == MOTOR_ID_X) ? &motor_x : &motor_y;

    /* 更新心跳 */
    motor->last_ack_time = get_system_tick();
    motor->is_online = 1;

    if (command_val == MOTOR_CMD_READ_POS && length == 8) {
        sign_flag = frame[2];
        raw_position = (frame[3] << 24) | (frame[4] << 16) | (frame[5] << 8) | frame[6];

        motor->current_angle = (float)raw_position / STEPMOTOR_ANGLE_READ_DIVIDER;

        if (sign_flag == 0x01) {
            motor->current_angle = -motor->current_angle;
        }

        if (fabs(motor->target_angle - motor->current_angle) <=
            STEPMOTOR_REACH_TOLERANCE) {
            motor->is_reached = 1;
        }
    } else if (command_val == MOTOR_CMD_MOVE_ACC || command_val == MOTOR_CMD_STOP ||
               command_val == MOTOR_CMD_SYNC_TRIG) {
        status_val = frame[2];

        if (status_val == MOTOR_STATUS_REACHED) {
            motor->is_reached = 1;
        } else if (status_val == MOTOR_STATUS_ERR1 || status_val == MOTOR_STATUS_ERR2) {
            motor->is_error = 1;
        }
    }
}

/**
 * @brief  心跳检测与轮询
 * @param  无
 * @retval 无
 */
static void heartbeat_check(void)
{
    uint8_t tx_buffer[3];
    uint32_t current_tick;

    current_tick = get_system_tick();

    if (current_tick - motor_comm.last_ping_time > STEPMOTOR_HEARTBEAT_PING_MS) {
        tx_buffer[0] = motor_comm.ping_target_id;
        tx_buffer[1] = MOTOR_CMD_READ_POS;
        tx_buffer[2] = MOTOR_CHECKSUM;

        push_tx_data(tx_buffer, 3);

        motor_comm.ping_target_id =
            (motor_comm.ping_target_id == MOTOR_ID_X) ? MOTOR_ID_Y : MOTOR_ID_X;
        motor_comm.last_ping_time = current_tick;
    }

    if (current_tick - motor_x.last_ack_time > STEPMOTOR_HEARTBEAT_TIMEOUT_MS) {
        motor_x.is_online = 0;
    }

    if (current_tick - motor_y.last_ack_time > STEPMOTOR_HEARTBEAT_TIMEOUT_MS) {
        motor_y.is_online = 0;
    }
}

/**
 * @brief  步进电机初始化
 * @param  huart  绑定的串口句柄
 * @retval 无
 */
void step_motor_init(UART_Regs *huart)
{
    uint32_t current_tick;

    if (!huart) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return;
    }

    /* 配置 UART 中断超时（用于 RX 超时检测） */
    DL_UART_Main_setRXInterruptTimeout(UART_STEPMOTOR_INST, 15);
    NVIC_EnableIRQ(UART_STEPMOTOR_INST_INT_IRQN);

    motor_comm.huart = huart;
    motor_comm.rx_read_pos = 0;
    motor_comm.rx_write_pos = 0;
    motor_comm.tx_read_pos = 0;
    motor_comm.tx_write_pos = 0;
    motor_comm.is_tx_busy = 0;
    motor_comm.rx_state = MOTOR_STATE_WAIT_ADDR;
    motor_comm.frame_index = 0;
    motor_comm.expected_frame_len = 0;

    start_dma_rx(motor_comm.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);

    current_tick = get_system_tick();
    motor_x.last_ack_time = current_tick;
    motor_y.last_ack_time = current_tick;
    motor_comm.last_ping_time = current_tick;
    motor_comm.ping_target_id = MOTOR_ID_X;

    /* 等待底板上电稳定（初始化阶段允许阻塞延时） */
    delay_cycles(STEPMOTOR_POWER_ON_DELAY_MS * 32000);

    step_motor_clear_zero(MOTOR_ID_X);
    step_motor_clear_zero(MOTOR_ID_Y);
}

/**
 * @brief  DMA 发送完成回调（ISR 中调用）
 * @param  huart  串口句柄
 * @retval 无
 */
void step_motor_tx_callback(UART_Regs *huart)
{
    if (huart == motor_comm.huart) {
        data_transmit();
    }
}

/**
 * @brief  UART 接收完成回调（ISR 中调用），将 DMA 数据推入 RX FIFO
 * @param  huart  串口句柄
 * @param  size   接收数据长度
 * @retval 无
 */
void step_motor_rx_callback(UART_Regs *huart, uint16_t size)
{
    uint16_t next;
    uint16_t i;

    if (huart != motor_comm.huart) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return;
    }

    if (size > 0) {
        for (i = 0; i < size; i++) {
            next = (motor_comm.rx_write_pos + 1) % STEPMOTOR_RX_FIFO_SIZE;

            if (next != motor_comm.rx_read_pos) {
                motor_comm.rx_fifo[motor_comm.rx_write_pos] = motor_comm.dma_rx_buffer[i];
                motor_comm.rx_write_pos = next;
            }
        }
    }

    memset(motor_comm.dma_rx_buffer, 0, STEPMOTOR_DMA_RX_BUF_SIZE);
    start_dma_rx(motor_comm.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);
}

/**
 * @brief  串口错误回调（ISR 中调用），复位接收状态机
 * @param  huart  串口句柄
 * @retval 无
 */
void step_motor_error_callback(UART_Regs *huart)
{
    if (huart != motor_comm.huart) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return;
    }

    motor_comm.rx_read_pos = 0;
    motor_comm.rx_write_pos = 0;
    motor_comm.rx_state = MOTOR_STATE_WAIT_ADDR;
    motor_comm.frame_index = 0;
    motor_comm.expected_frame_len = 0;

    memset(motor_comm.dma_rx_buffer, 0, STEPMOTOR_DMA_RX_BUF_SIZE);
    start_dma_rx(motor_comm.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);
}

/**
 * @brief  设置电机目标角度（X 固件梯形加减速位置模式，手册 §5.3.10）
 * @param  id         电机 ID
 * @param  mode       运动模式
 * @param  angle      目标角度
 * @param  speed      转速（RPM）
 * @param  accel      加速度（RPM/s）
 * @param  sync_flag  同步标志
 * @retval 0=成功，1=触发限位，2=异常拒执
 */
uint8_t step_motor_set_angle(uint8_t id, motor_move_mode_t mode, float angle,
    uint16_t speed, uint16_t accel, uint8_t sync_flag)
{
    step_motor_t *motor;
    float final_target;
    uint8_t limit_triggered;
    uint8_t tx_buffer[16];
    uint8_t direction;
    float abs_angle;
    uint32_t position;

    motor = (id == MOTOR_ID_X) ? &motor_x : &motor_y;

    if (!motor->is_online) {
        return 2;
    }
    if (isnan(angle) || isinf(angle)) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return 2;
    }

    if (speed > STEPMOTOR_MAX_SPEED_LIMIT) {
        speed = STEPMOTOR_MAX_SPEED_LIMIT;
    }

    final_target = angle;

    if (mode == MOTOR_MODE_ABS) {
        final_target = angle;
    } else if (mode == MOTOR_MODE_REL_PREV) {
        final_target = motor->target_angle + angle;
    } else if (mode == MOTOR_MODE_REL_CURR) {
        final_target = motor->current_angle + angle;
    }

    limit_triggered = 0;

    if (final_target > motor->max_angle || final_target < motor->min_angle) {
        limit_triggered = 1;
        {
            float boundary;

            boundary =
                (final_target > motor->max_angle) ? motor->max_angle : motor->min_angle;

            if (mode == MOTOR_MODE_ABS) {
                angle = boundary;
            } else if (mode == MOTOR_MODE_REL_PREV) {
                angle = boundary - motor->target_angle;
            } else if (mode == MOTOR_MODE_REL_CURR) {
                angle = boundary - motor->current_angle;
            }

            final_target = boundary;
        }
    }

    motor->target_angle = final_target;
    motor->is_reached = 0;

    direction = (angle >= 0) ? 0x00 : 0x01;
    abs_angle = fabs(angle) * STEPMOTOR_ANGLE_SEND_MULTIPLIER;

    if (abs_angle < 0.5f) {
        error_report(ERROR_SOURCE_STEP_MOTOR, DRV_ERR_PARAM);
        return limit_triggered;
    }

    position = (uint32_t)(abs_angle + 0.5f);

    tx_buffer[0] = id;
    tx_buffer[1] = MOTOR_CMD_MOVE_ACC;
    tx_buffer[2] = direction;
    tx_buffer[3] = (accel >> 8) & 0xFF;
    tx_buffer[4] = accel & 0xFF;
    tx_buffer[5] = (accel >> 8) & 0xFF;
    tx_buffer[6] = accel & 0xFF;
    tx_buffer[7] = (speed >> 8) & 0xFF;
    tx_buffer[8] = speed & 0xFF;
    tx_buffer[9] = (position >> 24) & 0xFF;
    tx_buffer[10] = (position >> 16) & 0xFF;
    tx_buffer[11] = (position >> 8) & 0xFF;
    tx_buffer[12] = position & 0xFF;
    tx_buffer[13] = (uint8_t)mode;
    tx_buffer[14] = sync_flag;
    tx_buffer[15] = MOTOR_CHECKSUM;

    push_tx_data(tx_buffer, 16);
    return limit_triggered;
}

/**
 * @brief  触发多机同步运动（手册 §5.3.14）
 * @param  无
 * @retval 无
 */
void step_motor_sync_trigger(void)
{
    uint8_t tx_buffer[4];

    tx_buffer[0] = MOTOR_ID_SYNC;
    tx_buffer[1] = MOTOR_CMD_SYNC_TRIG;
    tx_buffer[2] = MOTOR_PARAM_SYNC;
    tx_buffer[3] = MOTOR_CHECKSUM;

    push_tx_data(tx_buffer, 4);
}

/**
 * @brief  立即停止电机（手册 §5.3.13）
 * @param  id  电机 ID
 * @retval 无
 */
void step_motor_stop(uint8_t id)
{
    uint8_t tx_buffer[5];

    tx_buffer[0] = id;
    tx_buffer[1] = MOTOR_CMD_STOP;
    tx_buffer[2] = MOTOR_PARAM_STOP_1;
    tx_buffer[3] = MOTOR_PARAM_STOP_2;
    tx_buffer[4] = MOTOR_CHECKSUM;

    push_tx_data(tx_buffer, 5);
}

/**
 * @brief  清除当前位置零点（手册 §5.2.3）
 * @param  id  电机 ID
 * @retval 无
 */
void step_motor_clear_zero(uint8_t id)
{
    uint8_t tx_buffer[4];

    tx_buffer[0] = id;
    tx_buffer[1] = MOTOR_CMD_CLEAR_ZERO;
    tx_buffer[2] = MOTOR_PARAM_CLEAR;
    tx_buffer[3] = MOTOR_CHECKSUM;

    push_tx_data(tx_buffer, 4);
}

/**
 * @brief  数据解析任务（主循环中调用）
 * @note   包含软件 IDLE 检测、心跳检测和帧解析状态机
 * @param  无
 * @retval 无
 */
void step_motor_data_task(void)
{
    static uint16_t last_remain = STEPMOTOR_DMA_RX_BUF_SIZE;

    uint16_t remain;
    uint16_t rx_len;
    uint8_t read_byte;

    /* 软件 IDLE：DMA 余量连续两次不变则判定帧结束（TI 平台特化） */
    if (step_motor_check_idle_flag) {
        step_motor_check_idle_flag = 0;

        remain = DL_DMA_getTransferSize(DMA, DMA_CH_STEPMOTOR_RX_CHAN_ID);
        if (remain < STEPMOTOR_DMA_RX_BUF_SIZE) {
            if (remain == last_remain) {
                rx_len = STEPMOTOR_DMA_RX_BUF_SIZE - remain;
                step_motor_rx_callback(UART_STEPMOTOR_INST, rx_len);
            }
            last_remain = remain;
        } else {
            last_remain = STEPMOTOR_DMA_RX_BUF_SIZE;
        }
    }

    heartbeat_check();

    while (motor_comm.rx_read_pos != motor_comm.rx_write_pos) {
        read_byte = motor_comm.rx_fifo[motor_comm.rx_read_pos];
        motor_comm.rx_read_pos = (motor_comm.rx_read_pos + 1) % STEPMOTOR_RX_FIFO_SIZE;

        switch (motor_comm.rx_state) {
            case MOTOR_STATE_WAIT_ADDR:
                if (read_byte == MOTOR_ID_X || read_byte == MOTOR_ID_Y ||
                    read_byte == MOTOR_ID_SYNC) {
                    motor_comm.frame_index = 0;
                    motor_comm.frame_buffer[motor_comm.frame_index++] = read_byte;
                    motor_comm.rx_state = MOTOR_STATE_WAIT_CMD;
                }
                break;

            case MOTOR_STATE_WAIT_CMD:
                motor_comm.frame_buffer[motor_comm.frame_index++] = read_byte;

                if (read_byte == MOTOR_CMD_MOVE_ACC || read_byte == MOTOR_CMD_STOP ||
                    read_byte == MOTOR_CMD_SYNC_TRIG) {
                    motor_comm.expected_frame_len = 4;
                    motor_comm.rx_state = MOTOR_STATE_RECEIVING_DATA;
                } else if (read_byte == MOTOR_CMD_READ_POS) {
                    motor_comm.expected_frame_len = 8;
                    motor_comm.rx_state = MOTOR_STATE_RECEIVING_DATA;
                } else {
                    motor_comm.rx_state = MOTOR_STATE_WAIT_ADDR;
                    motor_comm.frame_index = 0;
                }
                break;

            case MOTOR_STATE_RECEIVING_DATA:
                motor_comm.frame_buffer[motor_comm.frame_index++] = read_byte;

                if (motor_comm.frame_index == motor_comm.expected_frame_len) {
                    if (motor_comm.frame_buffer[motor_comm.frame_index - 1] ==
                        MOTOR_CHECKSUM) {
                        process_reply(motor_comm.frame_buffer, motor_comm.frame_index);
                    }

                    motor_comm.rx_state = MOTOR_STATE_WAIT_ADDR;
                }
                break;

            default:
                motor_comm.rx_state = MOTOR_STATE_WAIT_ADDR;
                break;
        }
    }
}

/**
 * @brief  单轴相对移动
 * @param  id           电机 ID
 * @param  delta_angle  增量角度
 * @retval 无
 */
void step_motor_move_delta(uint8_t id, float delta_angle)
{
    step_motor_set_angle(id, MOTOR_MODE_REL_CURR, delta_angle,
        STEPMOTOR_TRACKING_DEFAULT_SPEED, STEPMOTOR_TRACKING_DEFAULT_ACC,
        STEPMOTOR_TRACKING_SYNC_FLAG_OFF);
}

/**
 * @brief  双轴相对移动（交替发送）
 * @param  delta_x  X 轴增量
 * @param  delta_y  Y 轴增量
 * @retval 无
 */
void step_motor_move_delta_sync(float delta_x, float delta_y)
{
    static uint8_t axis_toggle;

    if (axis_toggle == 0) {
        step_motor_set_angle(MOTOR_ID_X, MOTOR_MODE_REL_CURR, delta_x,
            STEPMOTOR_TRACKING_DEFAULT_SPEED, STEPMOTOR_TRACKING_DEFAULT_ACC,
            STEPMOTOR_TRACKING_SYNC_FLAG_OFF);
        axis_toggle = 1;
    } else {
        step_motor_set_angle(MOTOR_ID_Y, MOTOR_MODE_REL_CURR, delta_y,
            STEPMOTOR_TRACKING_DEFAULT_SPEED, STEPMOTOR_TRACKING_DEFAULT_ACC,
            STEPMOTOR_TRACKING_SYNC_FLAG_OFF);
        axis_toggle = 0;
    }
}

/**
 * @brief  获取 X 轴电机状态
 * @param  无
 * @retval X 轴电机状态结构体
 */
step_motor_t step_motor_get_x(void)
{
    return motor_x;
}

/**
 * @brief  获取 Y 轴电机状态
 * @param  无
 * @retval Y 轴电机状态结构体
 */
step_motor_t step_motor_get_y(void)
{
    return motor_y;
}
