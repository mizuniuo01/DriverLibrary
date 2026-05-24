#include "step_motor.h"

static StepMotor_Func_Struct motorInfo;

StepMotor_t MotorX = {
    .id = MOTOR_ID_X, 
    .target_angle = 0.0f, 
    .current_angle = 0.0f, 
    .is_reached = 1, 
    .max_angle = 900.0f, 
    .min_angle = -900.0f, 
    .last_ack_time = 0, 
    .is_online = 1, 
    .is_error = 0
};

StepMotor_t MotorY = {
    .id = MOTOR_ID_Y, 
    .target_angle = 0.0f, 
    .current_angle = 0.0f, 
    .is_reached = 1, 
    .max_angle = 10.0f,  
    .min_angle = -10.0f,  
    .last_ack_time = 0, 
    .is_online = 1, 
    .is_error = 0
};

/**
 * @brief 电机外设和协议通信初始化
 * @param huart 绑定的串口句柄指针
 * @retval None
 */
void StepMotor_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return;

    // 绑定外部传入的UART句柄用于底层交互
    motorInfo.huart = huart;

    // 软状态机与环形缓冲区(FIFO)管理指针重置初始化
    motorInfo.rx_read_pos = 0; 
    motorInfo.rx_write_pos = 0;
    motorInfo.tx_read_pos = 0; 
    motorInfo.tx_write_pos = 0;
    motorInfo.is_tx_busy = 0;
    
    motorInfo.rxState = MOTOR_STATE_WAIT_ADDR;
    motorInfo.frameIndex = 0;
    motorInfo.expected_frame_len = 0;
    
    // 清空上次状态并开启DMA空闲中断接收机制
    HAL_UARTEx_ReceiveToIdle_DMA(motorInfo.huart, motorInfo.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);
    
    uint32_t current_tick = HAL_GetTick();
    MotorX.last_ack_time = current_tick;
    MotorY.last_ack_time = current_tick;
    motorInfo.last_ping_time = current_tick;
    motorInfo.ping_target_id = MOTOR_ID_X;

    // 等待底板上电稳定验证
    HAL_Delay(STEPMOTOR_POWER_ON_DELAY_MS); 
    
    StepMotor_ClearZero(MOTOR_ID_X);
    StepMotor_ClearZero(MOTOR_ID_Y);
}

/**
 * @brief 从FIFO提取串口数据移交至DMA内存开始触发发送
 * @param None
 * @retval None
 */
static void StepMotor_DataTransmit(void)
{
    if (motorInfo.tx_write_pos == motorInfo.tx_read_pos)
    { 
        // 如果读写指针相等，说明队列已空，取消发送忙碌标志位
        motorInfo.is_tx_busy = 0; 
        return; 
    }

    motorInfo.is_tx_busy = 1;
    uint16_t sendLen = 0;
    
    if (motorInfo.tx_write_pos > motorInfo.tx_read_pos)
    {
        sendLen = motorInfo.tx_write_pos - motorInfo.tx_read_pos;
        
        if (sendLen > STEPMOTOR_DMA_TX_BUF_SIZE) 
        {
            sendLen = STEPMOTOR_DMA_TX_BUF_SIZE;
        }
        
        memcpy(motorInfo.dma_tx_buffer, &motorInfo.txFifo[motorInfo.tx_read_pos], sendLen);
        motorInfo.tx_read_pos += sendLen;
    }
    else
    {
        sendLen = STEPMOTOR_TX_FIFO_SIZE - motorInfo.tx_read_pos;
        
        if (sendLen > STEPMOTOR_DMA_TX_BUF_SIZE) 
        {
            sendLen = STEPMOTOR_DMA_TX_BUF_SIZE;
        }
        
        // 遇到尾部折返现象，先仅搬运指针到队尾这部分的数据空间
        memcpy(motorInfo.dma_tx_buffer, &motorInfo.txFifo[motorInfo.tx_read_pos], sendLen);
        motorInfo.tx_read_pos += sendLen;
        
        if (motorInfo.tx_read_pos >= STEPMOTOR_TX_FIFO_SIZE) 
        {
            motorInfo.tx_read_pos = 0;
        }
    }

    // 启动底层的串口DMA发生器
    HAL_UART_Transmit_DMA(motorInfo.huart, motorInfo.dma_tx_buffer, sendLen);
}

/**
 * @brief 将原数据压入并填充发送FIFO，同时检查触发发送调度
 * @param data 待压入的原始数据块首地址
 * @param len 将要填入缓冲区的长度
 * @retval None
 */
static void StepMotor_PushTxData(uint8_t *data, uint16_t len)
{
    // 关闭总中断，建立临界区保护FIFO的原子写入过程
    uint32_t primaskBit = __get_PRIMASK();
    __disable_irq();

    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t nextWritePos = (motorInfo.tx_write_pos + 1) % STEPMOTOR_TX_FIFO_SIZE;

        if (nextWritePos != motorInfo.tx_read_pos)
        {
            motorInfo.txFifo[motorInfo.tx_write_pos] = data[i];
            motorInfo.tx_write_pos = nextWritePos;
        }
    }

    // 临界区结束，恢复中断屏蔽器设存状态
    __set_PRIMASK(primaskBit);

    // 如果发送过程未启动执行，则立即触发下发调度
    if (motorInfo.is_tx_busy == 0) 
    {
        StepMotor_DataTransmit();
    }
}

/**
 * @brief TX_DMA发送完成中断回调中转站
 * @param huart 发生发送中断的串口句柄
 * @retval None
 */
void StepMotor_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || motorInfo.huart == NULL) return;

    if (huart->Instance == motorInfo.huart->Instance) 
    {
        // 当前数据块完毕重入函数，判断继续推空后续缓存
        StepMotor_DataTransmit();
    }
}

/**
 * @brief RX_DMA接收空闲超时回调中转站，用于数据清洗分流存放
 * @param huart 发生接收中断的串口句柄
 * @param Size 最近一次搬运总内存长
 * @retval None
 */
void StepMotor_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == NULL || motorInfo.huart == NULL) return;

    if (huart->Instance == motorInfo.huart->Instance)
    {
        if (Size > 0)
        {
            // 将本次DMA拿到的不定长突发数据，安全压入接收循环队列
            for (uint16_t i = 0; i < Size; i++)
            {
                uint16_t nextWritePos = (motorInfo.rx_write_pos + 1) % STEPMOTOR_RX_FIFO_SIZE;
                
                if (nextWritePos != motorInfo.rx_read_pos)
                {
                    motorInfo.rxFifo[motorInfo.rx_write_pos] = motorInfo.dma_rx_buffer[i];
                    motorInfo.rx_write_pos = nextWritePos;
                }
            }
        }
        
        // 此轮清理完毕，重置硬件资源与变量准备接收下一包
        memset(motorInfo.dma_rx_buffer, 0, STEPMOTOR_DMA_RX_BUF_SIZE);
        HAL_UARTEx_ReceiveToIdle_DMA(motorInfo.huart, motorInfo.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 串口错误回调
 * @param huart 发生错误的串口句柄
 * @retval None
 */
void StepMotor_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || motorInfo.huart == NULL) return;

    if (huart->Instance == motorInfo.huart->Instance)
    {
        motorInfo.rx_read_pos = 0; 
        motorInfo.rx_write_pos = 0;
        
        motorInfo.rxState = MOTOR_STATE_WAIT_ADDR;
        motorInfo.frameIndex = 0;
        motorInfo.expected_frame_len = 0;
        
        memset(motorInfo.dma_rx_buffer, 0, STEPMOTOR_DMA_RX_BUF_SIZE);
        
        HAL_UARTEx_ReceiveToIdle_DMA(motorInfo.huart, motorInfo.dma_rx_buffer, STEPMOTOR_DMA_RX_BUF_SIZE);
    }
}

/**
 * @brief 向下端驱动板发送带加减速曲线的角度定位动作指令
 * @param id 电机ID设备号
 * @param mode 梯形步线执行模式 (相对、绝对等)
 * @param angle 指定预期的目标角度值
 * @param speed 所用控制步进转速 (最大30000RPM)
 * @param accel 运行平滑度限制参数 (RPM/S)
 * @param sync_flag 同步标记指令开启位
 * @retval uint8_t 0-成功下发，1-触发钳位，2-异常拒执
 */
uint8_t StepMotor_SetAngle(uint8_t id, MotorMoveMode_t mode, float angle, uint16_t speed, uint16_t accel, uint8_t sync_flag)
{
    StepMotor_t *motor = (id == MOTOR_ID_X) ? &MotorX : &MotorY;
    
    // 离线状态或者非法数值主动拒执，保障安全
    if (motor->is_online == 0) return 2; 
    if (isnan(angle) || isinf(angle)) return 2; 
    
    // 对设定转速限制边界硬件能力封顶
    if (speed > STEPMOTOR_MAX_SPEED_LIMIT) 
    {
        speed = STEPMOTOR_MAX_SPEED_LIMIT;                   
    }

    // 按不同模式前瞻推算本次动作触发后的绝对坐标系定位
    float final_target = angle;
    
    if (mode == MOTOR_MODE_ABS)           
    {
        final_target = angle;                        
    }
    else if (mode == MOTOR_MODE_REL_PREV) 
    {
        final_target = motor->target_angle + angle;  
    }
    else if (mode == MOTOR_MODE_REL_CURR) 
    {
        final_target = motor->current_angle + angle; 
    }

    uint8_t limit_triggered = 0; 
    
    // 硬性限位软触发判断，截停到越界边界防止机械结构冲撞
    if (final_target > motor->max_angle || final_target < motor->min_angle) 
    {
        limit_triggered = 1; 
        float boundary = (final_target > motor->max_angle) ? motor->max_angle : motor->min_angle;
        
        if (mode == MOTOR_MODE_ABS)           
        {
            angle = boundary;
        }
        else if (mode == MOTOR_MODE_REL_PREV) 
        {
            angle = boundary - motor->target_angle;
        }
        else if (mode == MOTOR_MODE_REL_CURR) 
        {
            angle = boundary - motor->current_angle;
        }
        
        final_target = boundary;
    }

    motor->target_angle = final_target;
    motor->is_reached = 0;

    uint8_t tx_buffer[16];
    uint8_t direction = (angle >= 0) ? 0x00 : 0x01; 
    float abs_angle = fabs(angle) * ANGLE_SEND_MULTIPLIER;

    if (abs_angle < 0.5f)
    {
        // 小于0.005°的角度变化属于量化死区，直接忽略不用下发命令
        return limit_triggered;
    }

    uint32_t position = (uint32_t)(abs_angle + 0.5f);
    
    // 结构化填充通信协议，大端高低位拆分重组
    tx_buffer[0] = id;
    tx_buffer[1] = MOTOR_CMD_MOVE_ACC; 
    tx_buffer[2] = direction;
    tx_buffer[3] = (accel >> 8) & 0xFF; 
    tx_buffer[4] = accel & 0xFF;
    tx_buffer[5] = (accel >> 8) & 0xFF; 
    tx_buffer[6] = accel & 0xFF;
    tx_buffer[7] = (speed >> 8) & 0xFF; 
    tx_buffer[8] = speed & 0xFF;
    tx_buffer[9]  = (position >> 24) & 0xFF; 
    tx_buffer[10] = (position >> 16) & 0xFF;
    tx_buffer[11] = (position >> 8)  & 0xFF;
    tx_buffer[12] = position & 0xFF;
    tx_buffer[13] = (uint8_t)mode;      
    tx_buffer[14] = sync_flag;          
    tx_buffer[15] = MOTOR_CHECKSUM;     
    
    // 推送指令排队序列等待DMA收捕并报告执行成功情况
    StepMotor_PushTxData(tx_buffer, 16);
    return limit_triggered;
}

/**
 * @brief 执行之前发送过所有缓存包的预存同步触发
 * @param None
 * @retval None
 */
void StepMotor_SyncTrigger(void)
{
    uint8_t tx_buffer[4] = {MOTOR_ID_SYNC, MOTOR_CMD_SYNC_TRIG, MOTOR_PARAM_SYNC, MOTOR_CHECKSUM};
    
    StepMotor_PushTxData(tx_buffer, 4);
}

/**
 * @brief 对预制电机节点发送异常截停刹车包
 * @param id 目标电机ID
 * @retval None
 */
void StepMotor_Stop(uint8_t id)
{
    uint8_t tx_buffer[5] = {id, MOTOR_CMD_STOP, MOTOR_PARAM_STOP_1, MOTOR_PARAM_STOP_2, MOTOR_CHECKSUM}; 
    
    StepMotor_PushTxData(tx_buffer, 5);
}

/**
 * @brief 无条件清空电机当前的绝对记忆，将当下位置设零点
 * @param id 目标电机ID
 * @retval None
 */
void StepMotor_ClearZero(uint8_t id)
{
    uint8_t tx_buffer[4] = {id, MOTOR_CMD_CLEAR_ZERO, MOTOR_PARAM_CLEAR, MOTOR_CHECKSUM}; 
    
    StepMotor_PushTxData(tx_buffer, 4); 
}

/**
 * @brief 解析并验证一条数据栈命令执行底层更新
 * @param frame 等待被解析验证的缓冲协议段
 * @param length 数据片段总长度
 * @retval None
 */
static void StepMotor_ProcessReply(uint8_t *frame, uint8_t length)
{
    uint8_t receive_id  = frame[0];
    uint8_t command_val = frame[1];

    if (receive_id != MOTOR_ID_X && receive_id != MOTOR_ID_Y) return;
    StepMotor_t *motor = (receive_id == MOTOR_ID_X) ? &MotorX : &MotorY;
    
    // 只要有对应合法协议均记录节点保持在线心跳时间
    motor->last_ack_time = HAL_GetTick();
    motor->is_online = 1;

    if (command_val == MOTOR_CMD_READ_POS && length == 8)
    {
        // 浮点数反算解码机制，带有独立正负号补偿
        uint8_t sign_flag = frame[2];  
        uint32_t raw_position = (frame[3] << 24) | (frame[4] << 16) | (frame[5] << 8) | frame[6];
        
        motor->current_angle = (float)raw_position / ANGLE_READ_DIVIDER;  
        
        if (sign_flag == 0x01)
        { 
            motor->current_angle = -motor->current_angle; 
        }

        // 误差容限带内判定确认物理抵达
        if (fabs(motor->target_angle - motor->current_angle) <= STEPMOTOR_REACH_TOLERANCE)
        {
            motor->is_reached = 1;
        }
    }
    else if (command_val == MOTOR_CMD_MOVE_ACC || command_val == MOTOR_CMD_STOP || command_val == MOTOR_CMD_SYNC_TRIG)
    {
        uint8_t status_val = frame[2]; 
        
        if (status_val == MOTOR_STATUS_REACHED) 
        {
            motor->is_reached = 1;
        }
        else if (status_val == MOTOR_STATUS_ERR1 || status_val == MOTOR_STATUS_ERR2) 
        {
            motor->is_error = 1; 
        }
    }
}

/**
 * @brief 使用主状态机定时轮询获取在线状态的机制并更新超时变量
 * @param None
 * @retval None
 */
static void StepMotor_HeartbeatCheck(void)
{
    uint32_t current_tick = HAL_GetTick();
    
    // 根据询问周期对设备发起轮换状态探测
    if (current_tick - motorInfo.last_ping_time > HEARTBEAT_PING_MS)
    {
        uint8_t tx_buffer[3] = {motorInfo.ping_target_id, MOTOR_CMD_READ_POS, MOTOR_CHECKSUM};
        
        StepMotor_PushTxData(tx_buffer, 3);
        
        motorInfo.ping_target_id = (motorInfo.ping_target_id == MOTOR_ID_X) ? MOTOR_ID_Y : MOTOR_ID_X;
        motorInfo.last_ping_time = current_tick;
    }
    
    // 超过保活限值界线强制标识失联进行业务层降级或阻断拦截
    if (current_tick - MotorX.last_ack_time > HEARTBEAT_TIMEOUT_MS) 
    {
        MotorX.is_online = 0;
    }

    if (current_tick - MotorY.last_ack_time > HEARTBEAT_TIMEOUT_MS) 
    {
        MotorY.is_online = 0;
    }
}

/**
 * @brief 应部署在主循环内提取校验协议包的主轴核心业务
 * @param None
 * @retval None
 */
void StepMotor_Data_Task(void)
{
    StepMotor_HeartbeatCheck();

    while (motorInfo.rx_read_pos != motorInfo.rx_write_pos)
    {
        uint8_t read_byte = motorInfo.rxFifo[motorInfo.rx_read_pos];
        motorInfo.rx_read_pos = (motorInfo.rx_read_pos + 1) % STEPMOTOR_RX_FIFO_SIZE;

        switch (motorInfo.rxState)
        {
            case MOTOR_STATE_WAIT_ADDR:
            {
                if (read_byte == MOTOR_ID_X || read_byte == MOTOR_ID_Y || read_byte == MOTOR_ID_SYNC)
                {
                    motorInfo.frameIndex = 0;
                    motorInfo.frame_buffer[motorInfo.frameIndex++] = read_byte;
                    motorInfo.rxState = MOTOR_STATE_WAIT_CMD;
                }
                break;
            }
                
            case MOTOR_STATE_WAIT_CMD:
            {
                motorInfo.frame_buffer[motorInfo.frameIndex++] = read_byte;                
                
                if (read_byte == MOTOR_CMD_MOVE_ACC || read_byte == MOTOR_CMD_STOP || read_byte == MOTOR_CMD_SYNC_TRIG)
                {
                    motorInfo.expected_frame_len = 4; 
                    motorInfo.rxState = MOTOR_STATE_RECEIVING_DATA;
                }
                else if (read_byte == MOTOR_CMD_READ_POS)
                {
                    motorInfo.expected_frame_len = 8;
                    motorInfo.rxState = MOTOR_STATE_RECEIVING_DATA;
                }
                else
                {
                    motorInfo.rxState = MOTOR_STATE_WAIT_ADDR;
                    motorInfo.frameIndex = 0;
                }
                break;
            }
                
            case MOTOR_STATE_RECEIVING_DATA:
            {
                motorInfo.frame_buffer[motorInfo.frameIndex++] = read_byte;
                
                if (motorInfo.frameIndex == motorInfo.expected_frame_len)
                {
                    if (motorInfo.frame_buffer[motorInfo.frameIndex - 1] == MOTOR_CHECKSUM)
                    { 
                        StepMotor_ProcessReply(motorInfo.frame_buffer, motorInfo.frameIndex); 
                    }
                    
                    motorInfo.rxState = MOTOR_STATE_WAIT_ADDR; 
                }
                break;
            }
        }
    }
}

/**
 * @brief 请求系统利用步进执行单一方向相对位移
 * @param id 电机ID对应绑定标号
 * @param delta_angle 增量角度（正数增加值，负数减少值）
 * @retval None
 */
void StepMotor_MoveDelta(uint8_t id, float delta_angle)
{
    StepMotor_SetAngle(id, MOTOR_MODE_REL_CURR, delta_angle, TRACKING_DEFAULT_SPEED, TRACKING_DEFAULT_ACC, TRACKING_SYNC_FLAG_OFF);
}

/**
 * @brief 请求系统向双独立坐标同时推送相对位移命令以执行几何对角逼近
 * @param delta_x 偏航角横轴推算补偿
 * @param delta_y 俯仰角纵轴推算补偿
 * @retval None
 */
void StepMotor_MoveDelta_Sync(float delta_x, float delta_y)
{
    static uint8_t axis_toggle = 0; 

    if (axis_toggle == 0)
    {
        StepMotor_SetAngle(MOTOR_ID_X, MOTOR_MODE_REL_CURR, delta_x, 
                           TRACKING_DEFAULT_SPEED, TRACKING_DEFAULT_ACC, TRACKING_SYNC_FLAG_OFF);
        axis_toggle = 1;
    }
    else
    {
        StepMotor_SetAngle(MOTOR_ID_Y, MOTOR_MODE_REL_CURR, delta_y, 
                           TRACKING_DEFAULT_SPEED, TRACKING_DEFAULT_ACC, TRACKING_SYNC_FLAG_OFF);   
        axis_toggle = 0;
    }
}