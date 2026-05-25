#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

/* 缓冲区与系统参数 */
#define STEPMOTOR_DMA_RX_BUF_SIZE   128  /* DMA 接收单次最大量 */
#define STEPMOTOR_DMA_TX_BUF_SIZE   128  /* DMA 发送单次最大量 */
#define STEPMOTOR_RX_FIFO_SIZE      512  /* 接收环形队列容量 */
#define STEPMOTOR_TX_FIFO_SIZE      512  /* 发送环形队列容量 */
#define STEPMOTOR_MAX_FRAME_LEN     64   /* 单帧协议最大长度 */
#define STEPMOTOR_POWER_ON_DELAY_MS 50   /* 上电等待稳定延时 */
#define STEPMOTOR_MAX_SPEED_LIMIT   30000 /* 限速防爆保护(RPM) */
#define STEPMOTOR_REACH_TOLERANCE   0.1f /* 判定到达的容差角度 */

/* 电机 ID */
/*
 * 仅适配张大头 ZDT X42S 第二代步进电机控制器，
 * 使用指令模式（非脉冲模式）。
 * 通信协议：UART 二进制，地址+命令+数据+固定校验 0x6B。
 */

#define MOTOR_ID_X    0x01 /* X 轴 */
#define MOTOR_ID_Y    0x02 /* Y 轴 */
#define MOTOR_ID_SYNC 0x00 /* 广播/同步 */

/* 协议指令码 */
#define MOTOR_CHECKSUM     0x6B /* 固定校验字节 */
#define MOTOR_CMD_MOVE_ACC 0xFD /* 梯形曲线位置模式 */
#define MOTOR_CMD_STOP     0xFE /* 紧急停止 */
#define MOTOR_CMD_SYNC_TRIG 0xFF /* 多机同步触发 */
#define MOTOR_CMD_READ_POS 0x36 /* 读取实时位置 */
#define MOTOR_CMD_CLEAR_ZERO 0x0A /* 清除位置零点 */

/* 协议补充参数 */
#define MOTOR_PARAM_STOP_1 0x98 /* 急停参数位 1 */
#define MOTOR_PARAM_STOP_2 0x00 /* 急停参数位 2 */
#define MOTOR_PARAM_CLEAR  0x6D /* 清零参数位 */
#define MOTOR_PARAM_SYNC   0x66 /* 同步触发参数位 */

/* 应答状态 */
#define MOTOR_STATUS_REACHED 0x9F /* 已到达 */
#define MOTOR_STATUS_ERR1    0xE2 /* 堵转错误 1 */
#define MOTOR_STATUS_ERR2    0xEE /* 堵转错误 2 */

/* 角度换算 */
#define ANGLE_SEND_MULTIPLIER 100.0f /* 发送放大 100 倍 */
#define ANGLE_READ_DIVIDER    100.0f /* 接收缩小 100 倍 */

/* 心跳参数 */
#define HEARTBEAT_TIMEOUT_MS 1000 /* 超时判定离线（ms） */
#define HEARTBEAT_PING_MS    100  /* 轮询间隔（ms） */

/* 默认速度与加速度 */
#define TRACKING_DEFAULT_SPEED 800  /* 默认转速（RPM） */
#define TRACKING_DEFAULT_ACC   300  /* 默认加速度（RPM/s） */
#define TRACKING_SYNC_FLAG_ON  1    /* 同步开启 */
#define TRACKING_SYNC_FLAG_OFF 0    /* 同步关闭 */

/* 接收解析状态 */
typedef enum {
    MOTOR_STATE_WAIT_ADDR = 0,
    MOTOR_STATE_WAIT_CMD,
    MOTOR_STATE_RECEIVING_DATA,
} motor_rx_state_t;

/* 运动模式 */
typedef enum {
    MOTOR_MODE_REL_PREV = 0x00,
    MOTOR_MODE_ABS      = 0x01,
    MOTOR_MODE_REL_CURR = 0x02,
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

void step_motor_init(UART_HandleTypeDef *huart);
void step_motor_data_task(void);
void step_motor_rx_callback(UART_HandleTypeDef *huart, uint16_t size);
void step_motor_tx_callback(UART_HandleTypeDef *huart);
void step_motor_error_callback(UART_HandleTypeDef *huart);

uint8_t step_motor_set_angle(uint8_t id, motor_move_mode_t mode,
                             float angle, uint16_t speed,
                             uint16_t accel, uint8_t sync_flag);
void step_motor_sync_trigger(void);
void step_motor_stop(uint8_t id);
void step_motor_clear_zero(uint8_t id);
void step_motor_move_delta(uint8_t id, float delta_angle);
void step_motor_move_delta_sync(float delta_x, float delta_y);

step_motor_t step_motor_get_x(void);
step_motor_t step_motor_get_y(void);

#endif
