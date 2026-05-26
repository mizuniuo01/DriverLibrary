#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

#include <stdint.h>
#include <stm32f4xx_hal.h>
#include "drv_err.h"

/* 缓冲区与帧参数 */
typedef enum {
    STEPMOTOR_DMA_RX_BUF_SIZE = 128, /* DMA 接收单次最大量 */
    STEPMOTOR_DMA_TX_BUF_SIZE = 128, /* DMA 发送单次最大量 */
    STEPMOTOR_RX_FIFO_SIZE = 512,    /* 接收环形队列容量 */
    STEPMOTOR_TX_FIFO_SIZE = 512,    /* 发送环形队列容量 */
    STEPMOTOR_MAX_FRAME_LEN = 64,    /* 单帧协议最大长度 */
} stepmotor_buf_size_t;

#define STEPMOTOR_POWER_ON_DELAY_MS 50 /* 上电等待稳定延时 */
#define STEPMOTOR_MAX_SPEED_LIMIT \
    30000 /* 最大速度限制（手册：0~3000.0RPM，保留一位小数） */
#define STEPMOTOR_REACH_TOLERANCE 0.1f /* 判定到达的容差角度 */

/* 电机 ID */
/*
 * 仅适配张大头 ZDT X42S 第二代闭环步进电机控制器（X 固件）。
 * 使用指令模式（RS485/UART），不支持脉冲控制。
 * 协议细节以官方用户手册 V1.0.3 为准，以下注释仅供参考。
 *
 * 物理层：默认波特率 115200，8 数据位，1 停止位，无校验（§4.1）
 * 帧格式：地址(1B) + 命令(1B) + 数据(NB) + 校验(1B)（§4.1.1）
 *         地址 1~255，0 广播；校验默认固定字节 0x6B
 *
 * 本驱动使用 X 固件梯形加减速位置模式（FD 命令，§5.3.10）：
 *   16 字节帧：Addr + FD + 方向(1) + 加速加速度(2) + 减速加速度(2)
 *            + 最大速度(2) + 位置角度(4) + 运动模式(1) + 同步标志(1) + 6B
 *   本驱动将加减速加速度设为相同值以简化参数。
 */

/* 电机设备 ID */
typedef enum {
    MOTOR_ID_SYNC = 0x00, /* 广播地址，触发多机同步（§5.3.14） */
    MOTOR_ID_X = 0x01,    /* X 轴地址 */
    MOTOR_ID_Y = 0x02,    /* Y 轴地址 */
} motor_id_t;

/* 固定校验字节（§4.1.1） */
#define MOTOR_CHECKSUM 0x6B

/* 命令码（§5.2~§5.5） */
typedef enum {
    MOTOR_CMD_CLEAR_ZERO = 0x0A, /* 清除当前位置零点（§5.2.3） */
    MOTOR_CMD_READ_POS = 0x36,   /* 读取实时位置角度（§5.5.13） */
    MOTOR_CMD_MOVE_ACC = 0xFD,   /* 梯形加减速位置模式（X 固件 §5.3.10） */
    MOTOR_CMD_STOP = 0xFE,       /* 立即停止（§5.3.13） */
    MOTOR_CMD_SYNC_TRIG = 0xFF,  /* 触发多机同步运动（§5.3.14） */
} motor_cmd_t;

/* 命令辅助参数 */
typedef enum {
    MOTOR_PARAM_STOP_2 = 0x00, /* 同步标志：立即执行 */
    MOTOR_PARAM_SYNC = 0x66,   /* 同步触发辅助码（§5.3.14） */
    MOTOR_PARAM_CLEAR = 0x6D,  /* 清零辅助码（§5.2.3） */
    MOTOR_PARAM_STOP_1 = 0x98, /* 停止辅助码（§5.3.13） */
} motor_param_t;

/* 应答状态码（§4.1.2） */
typedef enum {
    MOTOR_STATUS_REACHED = 0x9F, /* 操作执行完成 */
    MOTOR_STATUS_ERR1 = 0xE2,    /* 参数超限/堵转/过流/过热保护触发 */
    MOTOR_STATUS_ERR2 = 0xEE,    /* 格式错误 */
} motor_status_t;

/* 角度换算 */
#define STEPMOTOR_ANGLE_SEND_MULTIPLIER 100.0f /* 发送放大 100 倍（0.1° 分辨率） */
#define STEPMOTOR_ANGLE_READ_DIVIDER 100.0f    /* 接收缩小 100 倍 */

/* 心跳参数 */
#define STEPMOTOR_HEARTBEAT_TIMEOUT_MS 1000 /* 超时判定离线（ms） */
#define STEPMOTOR_HEARTBEAT_PING_MS 100     /* 轮询间隔（ms） */

/* 默认速度与加速度（RPM / RPM/s） */
#define STEPMOTOR_TRACKING_DEFAULT_SPEED 800 /* 默认转速 */
#define STEPMOTOR_TRACKING_DEFAULT_ACC 300   /* 默认加速度 */

/* 同步标志 */
typedef enum {
    STEPMOTOR_TRACKING_SYNC_FLAG_OFF = 0, /* 同步关闭（立即执行） */
    STEPMOTOR_TRACKING_SYNC_FLAG_ON = 1,  /* 同步开启（先缓存，等 FF 触发） */
} stepmotor_sync_flag_t;

/* 接收解析状态 */
typedef enum {
    MOTOR_STATE_WAIT_ADDR = 0,
    MOTOR_STATE_WAIT_CMD,
    MOTOR_STATE_RECEIVING_DATA,
} motor_rx_state_t;

/* 运动模式（§5.3.10） */
typedef enum {
    MOTOR_MODE_REL_PREV = 0x00, /* 相对上一目标位置运动 */
    MOTOR_MODE_ABS = 0x01,      /* 相对坐标零点绝对运动 */
    MOTOR_MODE_REL_CURR = 0x02, /* 相对当前实时位置运动 */
} motor_move_mode_t;

/* 单电机状态 */
typedef struct {
    uint8_t id;
    float target_angle;
    float current_angle;
    uint8_t is_reached;

    float max_angle;
    float min_angle;

    uint32_t last_ack_time;
    uint8_t is_online;
    uint8_t is_error;
} step_motor_t;

/* 通信上下文 */
typedef struct {
    UART_HandleTypeDef *huart;

    uint8_t dma_rx_buffer[STEPMOTOR_DMA_RX_BUF_SIZE];
    uint8_t rx_fifo[STEPMOTOR_RX_FIFO_SIZE];
    uint16_t rx_write_pos;
    uint16_t rx_read_pos;

    uint8_t dma_tx_buffer[STEPMOTOR_DMA_TX_BUF_SIZE];
    uint8_t tx_fifo[STEPMOTOR_TX_FIFO_SIZE];
    uint16_t tx_write_pos;
    uint16_t tx_read_pos;
    volatile uint8_t is_tx_busy;

    motor_rx_state_t rx_state;
    uint8_t frame_buffer[STEPMOTOR_MAX_FRAME_LEN];
    uint16_t frame_index;
    uint8_t expected_frame_len;

    uint32_t last_ping_time;
    uint8_t ping_target_id;
} step_motor_comm_t;

drv_err_t step_motor_init(UART_HandleTypeDef *huart);
void step_motor_data_task(void);
void step_motor_rx_callback(UART_HandleTypeDef *huart, uint16_t size);
void step_motor_tx_callback(UART_HandleTypeDef *huart);
void step_motor_error_callback(UART_HandleTypeDef *huart);

uint8_t step_motor_set_angle(uint8_t id,
                             motor_move_mode_t mode,
                             float angle,
                             uint16_t speed,
                             uint16_t accel,
                             uint8_t sync_flag);
void step_motor_sync_trigger(void);
void step_motor_stop(uint8_t id);
void step_motor_clear_zero(uint8_t id);
void step_motor_move_delta(uint8_t id, float delta_angle);
void step_motor_move_delta_sync(float delta_x, float delta_y);

step_motor_t step_motor_get_x(void);
step_motor_t step_motor_get_y(void);

#endif
