#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#include "header.h"

#define ULTRASONIC_TRIG_PORT    GPIO_ULTRASONIC_TRIG_PORT
#define ULTRASONIC_TRIG_PIN     GPIO_ULTRASONIC_TRIG_PIN_3_PIN

#define ULTRASONIC_ECHO_PORT    GPIOB
#define ULTRASONIC_ECHO_PIN     GPIO_CAPTURE_ULTRASONIC_ECHO_C0_PIN

// 超声波定时器运行参数
#define ULTRASONIC_MAX_TIME_US  65535  // 超过 65.535ms 判定为超出距离
#define ULTRASONIC_PERIOD_MS    70     // 定时轮询周期不能小于 70ms
#define ULTRASONIC_SOUND_SPEED  340.0f // 声音传播速度 (m/s)

typedef enum 
{
    ULTRA_STATE_IDLE = 0,
    ULTRA_STATE_WAIT_ECHO         // 等待输入捕获接收
} Ultrasonic_State_t;

typedef struct
{
    float distance_mm;
    uint8_t is_valid;
} Ultrasonic_Data_Struct;

typedef struct 
{
    GPTIMER_Regs *htim;     // TI 定时器基址指针 (例如 TIMG0)
    uint32_t start_time;
    uint32_t end_time;
    
    uint32_t last_trigger_tick;
    volatile uint8_t capture_flag;   // 0: 空闲，1: 已捕获上升沿，2: 已捕获下降沿
    Ultrasonic_State_t state;
} Ultrasonic_Func_Struct;

extern Ultrasonic_Data_Struct ultrasonicData;

// 函数声明，需要在你存放系统滴答的地方提供一个获取毫秒的函数（或者用自定义全局变量 tick_ms）
extern uint32_t Get_SystemTick(void);

void Ultrasonic_Init(GPTIMER_Regs *htim);
void Ultrasonic_Task(void);
void Ultrasonic_CaptureCallback(GPTIMER_Regs *htim);

#endif