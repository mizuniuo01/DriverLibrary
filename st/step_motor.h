#ifndef __STEP_MOTOR_H
#define __STEP_MOTOR_H

#include "main.h"
#include "usart.h"
#include "debug.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// 缓冲区与系统宏定义
#define STEPMOTOR_DMA_RX_BUF_SIZE    128    // DMA接收单次最大量
#define STEPMOTOR_DMA_TX_BUF_SIZE    128    // DMA发送单次最大量
#define STEPMOTOR_RX_FIFO_SIZE       512    // 接收环形队列容量
#define STEPMOTOR_TX_FIFO_SIZE       512    // 发送环形队列容量
#define STEPMOTOR_MAX_FRAME_LEN      64     // 单帧协议最大长度 (状态包37字节)
#define STEPMOTOR_POWER_ON_DELAY_MS  50     // 上电等待稳定延时
#define STEPMOTOR_MAX_SPEED_LIMIT    30000  // 限速防爆保护(30000RPM)
#define STEPMOTOR_REACH_TOLERANCE    0.1f   // 判定到达的容差角度(0.1度)

// X固件节点相关宏定义
#define MOTOR_ID_X                   0x01   // X轴电机ID
#define MOTOR_ID_Y                   0x02   // Y轴电机ID
#define MOTOR_ID_SYNC                0x00   // 多机同步/广播ID

// X固件指令与参数宏定义
#define MOTOR_CHECKSUM               0x6B   // 固定校验字节 放在包尾
#define MOTOR_CMD_MOVE_ACC           0xFD   // 梯形曲线位置模式指令码
#define MOTOR_CMD_STOP               0xFE   // 紧急停止指令码
#define MOTOR_CMD_SYNC_TRIG          0xFF   // 触发多机同步缓存指令码
#define MOTOR_CMD_READ_POS           0x36   // 读取实时位置指令码
#define MOTOR_CMD_CLEAR_ZERO         0x0A   // 清除位置零点指令码

// X固件协议补充参数（用于组装固定的报文流）
#define MOTOR_PARAM_STOP_1           0x98   // 急停报文参数位1
#define MOTOR_PARAM_STOP_2           0x00   // 急停报文参数位2
#define MOTOR_PARAM_CLEAR            0x6D   // 清零报文强制参数位
#define MOTOR_PARAM_SYNC             0x66   // 同步触发报文强制参数位

// X固件应答状态解析宏定义
#define MOTOR_STATUS_REACHED         0x9F   // 返回包状态表示：已到达预期
#define MOTOR_STATUS_ERR1            0xE2   // 返回包状态表示：电机失步或堵转错误1
#define MOTOR_STATUS_ERR2            0xEE   // 返回包状态表示：电机失步或堵转错误2

// 换算与运行宏定义
#define ANGLE_SEND_MULTIPLIER        100.0f // 发送：放大100倍 (以0.01°为单位)
#define ANGLE_READ_DIVIDER           100.0f // 接收：缩小100倍 (以0.01°为单位)

#define HEARTBEAT_TIMEOUT_MS         1000   // 超过此时间无应答认定离线
#define HEARTBEAT_PING_MS            100     // 轮询心跳间隔(防RS485总线冲突)


// 基础移动缺省速度参数 (单位: 转速规划限值)Z
#define TRACKING_DEFAULT_SPEED 800
// 基础移动缺省加速度参数 (单位: RPM/S)
#define TRACKING_DEFAULT_ACC   300
// 移动命令同步标识位
#define TRACKING_SYNC_FLAG_ON  1
#define TRACKING_SYNC_FLAG_OFF 0

// 接收解析状态机枚举
typedef enum 
{
    MOTOR_STATE_WAIT_ADDR = 0, // 等待匹配电机地址
    MOTOR_STATE_WAIT_CMD,      // 等待提取命令字
    MOTOR_STATE_RECEIVING_DATA // 接收数据并校验
} MotorRxState_t;

// 运动模式枚举
typedef enum 
{
    MOTOR_MODE_REL_PREV = 0x00, // 相对于上一目标的相对运动
    MOTOR_MODE_ABS      = 0x01, // 相对于上电0点的绝对运动
    MOTOR_MODE_REL_CURR = 0x02  // 相对于当前实时的相对运动
} MotorMoveMode_t;

// 电机状态管理结构体
typedef struct 
{
    uint8_t id;             // 电机节点ID
    float target_angle;     // 推理的目标角度
    float current_angle;    // 实际响应的当前角度
    uint8_t is_reached;     // 状态：1-已到达，0-运动中
    
    float max_angle;        // 软限位：正向最大角度限制
    float min_angle;        // 软限位：负向最大角度限制
    
    uint32_t last_ack_time; // 心跳时间戳
    uint8_t is_online;      // 状态：1-在线，0-脱机
    uint8_t is_error;       // 状态：1-报错/堵转，0-正常
} StepMotor_t;

// 通信与缓冲状态上下文结构体
typedef struct 
{
    UART_HandleTypeDef *huart;                       // 绑定的串口句柄
    
    uint8_t dma_rx_buffer[STEPMOTOR_DMA_RX_BUF_SIZE];// DMA底层接收缓存区
    uint8_t rxFifo[STEPMOTOR_RX_FIFO_SIZE];          // 软件接收缓存队列
    uint16_t rx_write_pos;                           // 接收队列写指针
    uint16_t rx_read_pos;                            // 接收队列读指针
    
    uint8_t dma_tx_buffer[STEPMOTOR_DMA_TX_BUF_SIZE];// DMA底层发送缓存区
    uint8_t txFifo[STEPMOTOR_TX_FIFO_SIZE];          // 软件发送缓存队列
    uint16_t tx_write_pos;                           // 发送队列写指针
    uint16_t tx_read_pos;                            // 发送队列读指针
    volatile uint8_t is_tx_busy;                     // 发送线忙闲标志
    
    MotorRxState_t rxState;                          // 解析状态机步骤
    uint8_t frame_buffer[STEPMOTOR_MAX_FRAME_LEN];   // 单帧报文重组缓存
    uint16_t frameIndex;                             // 单帧拼接游标
    uint8_t expected_frame_len;                      // 动态判定的期望帧长
    
    uint32_t last_ping_time;                         // 上次发送探测回链的时间
    uint8_t ping_target_id;                          // 下一次发送探测的目标设备
} StepMotor_Func_Struct;

extern StepMotor_t MotorX;
extern StepMotor_t MotorY;

// 函数声明
void StepMotor_Init(UART_HandleTypeDef *huart);
void StepMotor_Data_Task(void);
void StepMotor_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void StepMotor_TxCpltCallback(UART_HandleTypeDef *huart);
void StepMotor_ErrorCallback(UART_HandleTypeDef *huart);

uint8_t StepMotor_SetAngle(uint8_t id, MotorMoveMode_t mode, float angle, uint16_t speed, uint16_t accel, uint8_t sync_flag);
void StepMotor_SyncTrigger(void);
void StepMotor_Stop(uint8_t id);
void StepMotor_ClearZero(uint8_t id);

void StepMotor_MoveDelta(uint8_t id, float delta_angle);
void StepMotor_MoveDelta_Sync(float delta_x, float delta_y);

#endif