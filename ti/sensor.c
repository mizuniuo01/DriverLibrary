#include "sensor.h"

static I2C_Regs *sensor_hi2c = NULL;
static uint8_t sensor_rx_buffer[1] = {0};
static volatile uint8_t sensor_processed_data = 0;

// 控制执行流的轮询状态切换枚举标识
typedef enum {
    MODE_IDLE = 0,
    MODE_SEND_CMD,
    MODE_WAIT_READ
} Sensor_Pol_State_t;

static volatile Sensor_Pol_State_t pol_state = MODE_IDLE;

/**
 * @brief 灰度传感器的系统外设接口环境初定位
 * @param hi2c I2C 外设分配句柄
 * @retval None
 */
void Sensor_Init(I2C_Regs *hi2c)
{
    if (hi2c == NULL) return;

    NVIC_EnableIRQ(I2C_SENSOR_INST_INT_IRQN);
    DL_I2C_setTimeoutACount(I2C_SENSOR_INST, 0xFFFFF);
    DL_I2C_enableTimeoutA(I2C_SENSOR_INST);
    DL_I2C_enableInterrupt(I2C_SENSOR_INST, DL_I2C_INTERRUPT_TIMEOUT_A | DL_I2C_INTERRUPT_CONTROLLER_NACK | DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST);
    NVIC_SetPriority(I2C_SENSOR_INST_INT_IRQN, 3);

    sensor_hi2c = hi2c;
    pol_state = MODE_IDLE;
}

/**
 * @brief 报告检测机运行忙或闲的工参
 * @param None
 * @retval Sensor_State_t 当前传感器是否正忙或自由
 */
Sensor_State_t Sensor_GetState(void) 
{
    return (pol_state == MODE_IDLE) ? SENSOR_IDLE : SENSOR_BUSY;
}

/**
 * @brief 通过计时器周期进入推进的 I2C 查询状态流，非阻塞
 * @param None
 * @retval None
 */
void Sensor_RequestData_DMA(void)
{
    if (sensor_hi2c == NULL) return;

    if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_ERROR)
    {
        Sensor_ErrorCallback(sensor_hi2c);
        return;
    }

    switch (pol_state)
    {
        case MODE_IDLE:
        {
            if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_IDLE)
            {
                DL_I2C_clearInterruptStatus(sensor_hi2c, 0xFFFFFFFF);
                
                DL_I2C_fillControllerTXFIFO(sensor_hi2c, &((uint8_t){SENSOR_CMD_READ_DIG}), 1);
                
                DL_I2C_startControllerTransfer(sensor_hi2c, SENSOR_I2C_ADDR_7BIT, DL_I2C_CONTROLLER_DIRECTION_TX, 1);
                
                pol_state = MODE_SEND_CMD;
            }
            break;
        }

        case MODE_SEND_CMD:
        {
            if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
                if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_ERROR) {
                    Sensor_ErrorCallback(sensor_hi2c);
                }
                return;
            }
            
            DL_I2C_startControllerTransfer(sensor_hi2c, SENSOR_I2C_ADDR_7BIT, DL_I2C_CONTROLLER_DIRECTION_RX, 1);
            pol_state = MODE_WAIT_READ;
            break;
        }

        case MODE_WAIT_READ:
        {
            // 等待 RX FIFO 中出现数据
            if (DL_I2C_isControllerRXFIFOEmpty(sensor_hi2c)) 
            {
                if (DL_I2C_getControllerStatus(sensor_hi2c) & DL_I2C_CONTROLLER_STATUS_ERROR) {
                    Sensor_ErrorCallback(sensor_hi2c);
                }
                return;
            }
            
            sensor_rx_buffer[0] = DL_I2C_receiveControllerData(sensor_hi2c);

            uint8_t raw_data = sensor_rx_buffer[0];
            uint8_t temp_data = 0;
            
            for (uint8_t i = 0; i < 8; i++)
            {
                if (((raw_data >> i) & 0x01) == 0)
                {
                    temp_data |= (1 << i);
                }
            }
            
            sensor_processed_data = temp_data;
            
            pol_state = MODE_IDLE;
            break;
        }
    }
}

/**
 * @brief I2C 接收完成回调
 * @param hi2c I2C 句柄指针
 * @retval None
 */
void Sensor_RxCpltCallback(I2C_Regs *hi2c)
{
    if (hi2c == NULL || hi2c != sensor_hi2c) return;

    pol_state = MODE_IDLE;
}

/**
 * @brief I2C 错误回调
 * @param hi2c I2C 句柄指针
 * @retval None
 */
void Sensor_ErrorCallback(I2C_Regs *hi2c)
{
    if (hi2c == NULL || hi2c != sensor_hi2c) return;

    DL_I2C_resetControllerTransfer(hi2c);
    DL_I2C_clearInterruptStatus(hi2c, 0xFFFFFFFF);
    pol_state = MODE_IDLE;
}


uint8_t Sensor_ReadData(void)
{ 
    return sensor_processed_data; 
}