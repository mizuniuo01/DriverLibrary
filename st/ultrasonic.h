#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#include "main.h"

#define ULTRASONIC_TRIG_PORT ultrasonic_Trig_GPIO_Port
#define ULTRASONIC_TRIG_PIN  ultrasonic_Trig_Pin

// 超声波定时器运行参数
#define ULTRASONIC_MAX_TIME_US  66000  // 超过 66ms 判定为超出距离
#define ULTRASONIC_PERIOD_MS    70     // 定时轮询周期必须大于最短盲区 70ms
#define ULTRASONIC_SOUND_SPEED  340.0f // 声音传播速度 (m/s)

typedef enum 
{
    ULTRA_STATE_IDLE = 0,
    ULTRA_STATE_WAIT_TRIGGER_END, // 用以发出 10+us 的高电平触发信号
    ULTRA_STATE_WAIT_ECHO         // 用以等待输入捕获接收
} Ultrasonic_State_t;

typedef struct
{
    float distance_mm;
    uint8_t is_valid;
} Ultrasonic_Data_Struct;

typedef struct 
{
    TIM_HandleTypeDef *htim;
    uint32_t start_time;
    uint32_t end_time;
    
    uint32_t last_trigger_tick;
    uint8_t capture_flag;         // 0: 空闲，1: 已捕获上升沿， 等待下降沿
    Ultrasonic_State_t state;
} Ultrasonic_Func_Struct;

extern Ultrasonic_Data_Struct ultrasonicData;

// 函数声明
void Ultrasonic_Init(TIM_HandleTypeDef *htim);
void Ultrasonic_Task(void);
void Ultrasonic_CaptureCallback(TIM_HandleTypeDef *htim);

#endif