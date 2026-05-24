#include "encoder.h"

/*
 * 编码器说明：
 * - 左轮使用定时器QEI模式，硬件4x计数，软件/2降低精度与右轮对齐
 * - 右轮使用定时器A相双边沿捕获，软件计算增量，方向由电机控制侧软件标志位统一给出 
 * - 右轮捕获中断服务函数中仅更新捕获计数，增量计算在主循环中获取右轮数据时进行
*/

volatile Encoder_Data_t encoderData = {0, 0};
volatile uint16_t timer_capture_right_count = 0;

/**
 * @brief 编码器初始化
 * @param htim_left_qei 左轮QEI定时器句柄
 * @param htim_right_capture 右轮捕获定时器句柄
 * @retval None
 */
void Encoder_Init(GPTIMER_Regs *htim_left_qei, GPTIMER_Regs *htim_right_capture)
{
    if (htim_left_qei != NULL)
    {
        DL_Timer_setTimerCount(htim_left_qei, 0);
        DL_Timer_startCounter(htim_left_qei);
    }

    if (htim_right_capture != NULL)
    {
        DL_TimerA_stopCounter(htim_right_capture);
        DL_TimerA_disableClock(htim_right_capture);
        DL_TimerA_setCounterMode(htim_right_capture, DL_TIMER_COUNT_MODE_UP);
        DL_Timer_setTimerCount(htim_right_capture, 0);

        /* A相双边沿捕获 */
        DL_TimerA_setCaptureCompareCtl(htim_right_capture,
            DL_TIMER_CC_MODE_CAPTURE,
            (DL_TIMER_CC_ZCOND_NONE |
             DL_TIMER_CC_ACOND_TIMCLK |
             DL_TIMER_CC_LCOND_NONE |
             DL_TIMER_CC_CCOND_TRIG_EDGE),
            DL_TIMER_CC_0_INDEX);

        /* CC1禁用捕获，方向由电机控制侧软件标志位统一给出 */
        DL_TimerA_setCaptureCompareCtl(htim_right_capture,
            DL_TIMER_CC_MODE_CAPTURE,
            (DL_TIMER_CC_ZCOND_NONE |
             DL_TIMER_CC_ACOND_TIMCLK |
             DL_TIMER_CC_LCOND_NONE |
             DL_TIMER_CC_CCOND_NOCAPTURE),
            DL_TIMER_CC_1_INDEX);

        DL_GPIO_initDigitalInputFeatures(
            ENCODER_RIGHT_DIR_PIN_1_IOMUX,
            DL_GPIO_INVERSION_DISABLE,
            DL_GPIO_RESISTOR_NONE,
            DL_GPIO_HYSTERESIS_DISABLE,
            DL_GPIO_WAKEUP_DISABLE);

        DL_TimerA_enableClock(htim_right_capture);
        DL_TimerA_startCounter(htim_right_capture);

        DL_TimerA_disableInterrupt(htim_right_capture,
            DL_TIMERA_INTERRUPT_CC0_DN_EVENT |
            DL_TIMERA_INTERRUPT_CC0_UP_EVENT |
            DL_TIMERA_INTERRUPT_CC1_DN_EVENT |
            DL_TIMERA_INTERRUPT_CC1_UP_EVENT);

        DL_TimerA_enableInterrupt(htim_right_capture,
            DL_TIMERA_INTERRUPT_CC0_DN_EVENT |
            DL_TIMERA_INTERRUPT_CC0_UP_EVENT);

        NVIC_EnableIRQ(CAPTURE_ENCODER_RIGHT_INST_INT_IRQN);
    }
}

/**
 * @brief 获取左轮增量计数
 * @param htim 左轮QEI定时器句柄
 * @retval none
 */
void Encoder_Get_Left(GPTIMER_Regs *htim)
{
    static uint16_t last_count_left = 0;
    uint16_t current_count = 0;
    int16_t diff_value = 0;

    if (htim == NULL)
    {
        return;
    }

    current_count = (uint16_t)DL_Timer_getTimerCount(htim);
    diff_value = (int16_t)(current_count - last_count_left);

    /* 左轮QEI为4x计数，/2与右轮口径对齐；取反修正安装方向 */
    encoderData.left_val = (int16_t)(-(diff_value / 2));
    last_count_left = current_count;
}

/**
 * @brief 获取右轮增量计数
 * @param None
 * @retval none
 */
void Encoder_Get_Right(void)
{
    static uint16_t last_count_right = 0;
    uint32_t primaskBit = __get_PRIMASK();
    uint16_t current_count = 0;
    int16_t diff_value = 0;

    __disable_irq();
    current_count = timer_capture_right_count;
    __set_PRIMASK(primaskBit);

    diff_value = (int16_t)(current_count - last_count_right);

    if (motor_right_direction_sign < 0)
    {
        diff_value = -diff_value;
    }

    encoderData.right_val = diff_value;

    last_count_right = current_count;
}

/**
 * @brief 右轮捕获中断服务
 * @param None
 * @retval None
 */
void CAPTURE_ENCODER_RIGHT_INST_IRQHandler(void)
{
    DL_TIMER_IIDX pending = DL_Timer_getPendingInterrupt(CAPTURE_ENCODER_RIGHT_INST);

    while ((pending == DL_TIMER_IIDX_CC0_UP) || (pending == DL_TIMER_IIDX_CC0_DN))
    {
        (void)DL_TimerA_getCaptureCompareValue(CAPTURE_ENCODER_RIGHT_INST,
            DL_TIMER_CC_0_INDEX);

        timer_capture_right_count++;

        pending = DL_Timer_getPendingInterrupt(CAPTURE_ENCODER_RIGHT_INST);
    }
}
