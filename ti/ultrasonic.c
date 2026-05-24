#include "ultrasonic.h"

static Ultrasonic_Func_Struct ultraInfo;
Ultrasonic_Data_Struct ultrasonicData = {0.0f, 0};

#define DELAY_10US()    delay_cycles(320)

/**
 * @brief 超声波模块初始化
 * @param htim 输入捕获所用定时器基址指针
 * @retval None
 */
void Ultrasonic_Init(GPTIMER_Regs *htim)
{
    if (htim == NULL) return;

    NVIC_EnableIRQ(CAPTURE_ULTRASONIC_ECHO_INST_INT_IRQN);

    ultraInfo.htim = htim;
    ultraInfo.state = ULTRA_STATE_IDLE;
    ultraInfo.capture_flag = 0;
    ultraInfo.last_trigger_tick = Get_SystemTick();

    // 开启定时器
    DL_Timer_startCounter(ultraInfo.htim);
}

/**
 * @brief 超声波双边沿捕获中断回调程序
 * @param htim 定时器句柄，用于区分不同的中断源
 * @retval None
 */
void Ultrasonic_CaptureCallback(GPTIMER_Regs *htim)
{
    if (htim == NULL || ultraInfo.htim == NULL) return;

    if (htim == ultraInfo.htim)
    {
        // 提取 CC0 通道的捕获值
        uint32_t val = DL_Timer_getCaptureCompareValue(htim, DL_TIMER_CC_0_INDEX);
        
        if (ultraInfo.state == ULTRA_STATE_WAIT_ECHO)
        {
            if (ultraInfo.capture_flag == 0)
            {
                ultraInfo.start_time = val;
                ultraInfo.capture_flag = 1;
            }
            else if (ultraInfo.capture_flag == 1)
            {
                ultraInfo.end_time = val;
                ultraInfo.capture_flag = 2; 
            }
        }
    }
}

/**
 * @brief 超声波任务控制机，部署于主循环
 * @param None
 * @retval None
 */
void Ultrasonic_Task(void)
{
    uint32_t currentTick = Get_SystemTick();

    switch (ultraInfo.state)
    {
        case ULTRA_STATE_IDLE:
        {
            if (currentTick - ultraInfo.last_trigger_tick >= ULTRASONIC_PERIOD_MS)
            {
                ultraInfo.last_trigger_tick = currentTick;
                ultraInfo.capture_flag = 0; 
                
                // 发送 10 微秒的高电平 TRIG 脉冲
                DL_GPIO_setPins(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN);
                DELAY_10US(); 
                DL_GPIO_clearPins(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN);
                
                ultraInfo.state = ULTRA_STATE_WAIT_ECHO;
            }
            break;
        }

        case ULTRA_STATE_WAIT_ECHO:
        {
            if (ultraInfo.capture_flag == 2)
            {
                uint32_t duration = 0;
                
                // 处理计数器向上计数的差值
                if (ultraInfo.end_time >= ultraInfo.start_time)
                {
                    duration = ultraInfo.end_time - ultraInfo.start_time;
                }
                else
                {
                    duration = (CAPTURE_ULTRASONIC_ECHO_INST_LOAD_VALUE - ultraInfo.start_time) + ultraInfo.end_time + 1;
                }
                
                // HC-SR04 超范围或无回波时通常为 66ms。这里卡死在有效范围即可
                if (duration > 0 && duration < ULTRASONIC_MAX_TIME_US)
                {
                    // 距离 (mm) = 持续时间(us) * 340m/s / 2
                    // 1us * 0.34mm/us / 2 = 0.17
                    ultrasonicData.distance_mm = (float)duration * 0.17f; 
                    ultrasonicData.is_valid = 1;
                }
                else
                {
                    ultrasonicData.is_valid = 0;
                }
                
                ultraInfo.state = ULTRA_STATE_IDLE;
            }
            // 超时清零重置
            else if (currentTick - ultraInfo.last_trigger_tick >= ULTRASONIC_PERIOD_MS)
            {
                ultrasonicData.is_valid = 0;
                ultraInfo.state = ULTRA_STATE_IDLE;
                ultraInfo.capture_flag = 0;
            }
            break;
        }
    }
}