#include "ultrasonic.h"

static Ultrasonic_Func_Struct ultraInfo;
Ultrasonic_Data_Struct ultrasonicData = {0.0f, 0};

/**
 * @brief 超声波模块初始化
 * @param htim 输入捕获所用定时器句柄（如 &htim2）
 * @retval None
 */
void Ultrasonic_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL) return;

    ultraInfo.htim = htim;
    ultraInfo.state = ULTRA_STATE_IDLE;
    ultraInfo.capture_flag = 0;
    ultraInfo.last_trigger_tick = HAL_GetTick();

    // 默认开启上升沿输入捕获中断
    HAL_TIM_IC_Start_IT(ultraInfo.htim, TIM_CHANNEL_1);
}

/**
 * @brief 超声波定时捕获中断回调程序
 * @param htim 定时器句柄，用于区分不同的中断源
 * @retval None
 */
void Ultrasonic_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim == NULL || ultraInfo.htim == NULL) return;

    if (htim->Instance == ultraInfo.htim->Instance)
    {
        if (ultraInfo.capture_flag == 0)
        {
            ultraInfo.start_time = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            ultraInfo.capture_flag = 1;
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
        }
        else if (ultraInfo.capture_flag == 1)
        {
            ultraInfo.end_time = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
            ultraInfo.capture_flag = 2; 
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
        }
    }
}

/**
 * @brief 超声波任务控制机，部署于主循环 (非阻塞设计，具备超时自愈)
 * @param None
 * @retval None
 */
void Ultrasonic_Task(void)
{
    uint32_t currentTick = HAL_GetTick();

    switch (ultraInfo.state)
    {
        case ULTRA_STATE_IDLE:
        {
            if (currentTick - ultraInfo.last_trigger_tick >= ULTRASONIC_PERIOD_MS)
            {
                HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_SET);
                ultraInfo.last_trigger_tick = currentTick;
                
                // 为了防止错乱，在此强行将捕获极性重置为上升沿
                __HAL_TIM_SET_CAPTUREPOLARITY(ultraInfo.htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
                ultraInfo.capture_flag = 0; 
                
                ultraInfo.state = ULTRA_STATE_WAIT_TRIGGER_END;
            }
            break;
        }

        case ULTRA_STATE_WAIT_TRIGGER_END:
        {
            if (currentTick - ultraInfo.last_trigger_tick >= 1)
            {
                HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);
                ultraInfo.state = ULTRA_STATE_WAIT_ECHO;
            }
            break;
        }

        case ULTRA_STATE_WAIT_ECHO:
        {
            if (ultraInfo.capture_flag == 2)
            {
                uint32_t duration = 0;
                
                if (ultraInfo.end_time >= ultraInfo.start_time)
                {
                    duration = ultraInfo.end_time - ultraInfo.start_time;
                }
                else
                {
                    duration = (0xFFFFFFFF - ultraInfo.start_time) + ultraInfo.end_time + 1;
                }

                if (duration > 0 && duration < ULTRASONIC_MAX_TIME_US)
                {
                    float dist_mm = (float)duration * (ULTRASONIC_SOUND_SPEED / 1000.0f) / 2.0f;
                    ultrasonicData.distance_mm = dist_mm; 
                    ultrasonicData.is_valid = 1;
                }
                else
                {
                    ultrasonicData.is_valid = 0;
                }
                
                ultraInfo.state = ULTRA_STATE_IDLE;
            }
            // 超时保底处理：如果触发一直处于测距周期并且拿不到第二个中断下降沿
            else if (currentTick - ultraInfo.last_trigger_tick >= ULTRASONIC_PERIOD_MS)
            {
                ultrasonicData.is_valid = 0;
                ultraInfo.state = ULTRA_STATE_IDLE;
                ultraInfo.capture_flag = 0;
                // 复位为上升沿以备下一轮捕捉
                __HAL_TIM_SET_CAPTUREPOLARITY(ultraInfo.htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
            }
            break;
        }
    }
}