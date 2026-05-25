/**
 * @file    motor.c
 * @brief   直流有刷电机驱动模块（IN/IN 模式，两路 PWM）
 * @author  mizuniuo01
 * @date    2026-05-25
 * @version 1.0.0
 * @note    左电机使用 PWM CH0，右电机使用 PWM CH1
 * @note    左右电机方向引脚逻辑相反（机械安装方向导致）
 * @note    右电机方向标志位供 encoder 模块获取编码器方向
 * @note    依赖：PWM 模块（pwm_set_compare_ch0/ch1）
 * @warning 参数 speed 为直接 PWM 比较值，非物理速度
 *
 * @usage
 * ─────────────────────────────────────────────────────────
 * 适配两路直流有刷电机 + 两片 DRV8874 驱动芯片，与 pwm、encoder
 * 模块配合构成完整的电机闭环控制链路。
 *
 * 速度参数基于 20kHz PWM 载波频率：
 *   PWM_MAX_COMPARE = 定时器时钟 / 20000 = 2000（40MHz 时钟）
 *   MOTOR_MAX_SPEED = PWM_MAX_COMPARE（100% 占空比）
 *   MOTOR_EFFECTIVE_MIN_SPEED = 60（3%，低于此值电机不转的死区）
 *
 * ── 硬件拓扑（DRV8874 IN/IN 模式）──
 *
 *   MCU TIMG CH0 ──────────→ DRV8874#1 IN1  (PWM)
 *   MCU GPIO PH_IN2 ───────→ DRV8874#1 IN2  (方向)
 *   MCU GPIO nSLEEP ───────→ DRV8874#1 nSLEEP
 *   DRV8874#1 OUT1/OUT2 ───→ 左电机
 *
 *   MCU TIMG CH1 ──────────→ DRV8874#2 IN1  (PWM)
 *   MCU GPIO PH_IN2 ───────→ DRV8874#2 IN2  (方向)
 *   MCU GPIO nSLEEP ───────→ DRV8874#2 nSLEEP
 *   DRV8874#2 OUT1/OUT2 ───→ 右电机
 *
 * 左右电机机械对向安装，因此方向引脚逻辑相反：
 * - 左电机正向 = PH_IN2 置高
 * - 右电机正向 = PH_IN2 清零
 *
 * ── 初始化 ──
 *
 * static motor_handle_t motor;
 *
 * void system_init(void)
 * {
 *     motor_cfg_t cfg = {
 *         .port          = GPIO_MOTOR_PORT_PORT,
 *         .l_nsleep_pin  = GPIO_MOTOR_PORT_L_nSLEEP_PIN,
 *         .r_nsleep_pin  = GPIO_MOTOR_PORT_R_nSLEEP_PIN,
 *         .l_ph_in2_pin  = GPIO_MOTOR_PORT_L_PH_IN2_PIN,
 *         .r_ph_in2_pin  = GPIO_MOTOR_PORT_R_PH_IN2_PIN,
 *     };
 *
 *     pwm_init(PWM_MOTOR_INST);
 *     motor_init(&motor, &cfg);
 *     encoder_init(ENCODER_LEFT_QEI_INST,
 *                  ENCODER_RIGHT_CAPTURE_INST);
 * }
 *
 * ── 速度控制 ──
 *
 * // PWM 量程 -2000 ~ +2000，符号决定方向，绝对值决定占空比
 * motor_set_speed_left(&motor, PWM_MOTOR_INST,  1000);  // 左轮 50%
 * motor_set_speed_right(&motor, PWM_MOTOR_INST, -1500); // 右轮 75% 反转
 *
 * // 低于 MOTOR_EFFECTIVE_MIN_SPEED(60) 的值自动输出 0
 *
 * ── 闭环控制链路 ──
 *
 *   encoder_get_left/right()  →  PID  →  motor_set_speed_left/right()
 *        ↑                                  │
 *        │                                  ↓
 *   encoder_scan_left/right()          pwm_set_compare_ch0/ch1()
 *        ↑                                  │
 *        │                                  ↓
 *   [编码器硬件]                      [DRV8874 → 电机]
 */

#include "motor.h"

#include "pwm.h"

static volatile int8_t motor_right_direction_sign = 1;

/**
 * @brief  电机初始化（唤醒驱动芯片）
 * @param  handle  电机句柄指针
 * @param  cfg     电机配置指针
 * @retval 无
 */
void motor_init(motor_handle_t *handle, const motor_cfg_t *cfg)
{
    if (!handle || !cfg) {
        return;
    }

    handle->port = cfg->port;
    handle->l_nsleep_pin = cfg->l_nsleep_pin;
    handle->r_nsleep_pin = cfg->r_nsleep_pin;
    handle->l_ph_in2_pin = cfg->l_ph_in2_pin;
    handle->r_ph_in2_pin = cfg->r_ph_in2_pin;

    motor_right_direction_sign = 1;

    /* 拉高 nSLEEP 使驱动芯片脱离待机模式 */
    DL_GPIO_setPins(handle->port, handle->l_nsleep_pin | handle->r_nsleep_pin);
}

/**
 * @brief  设置左电机速度
 * @param  handle  电机句柄指针
 * @param  htim    PWM 定时器句柄
 * @param  speed   PWM 比较值（-PWM_MAX_COMPARE ~ +PWM_MAX_COMPARE）
 * @retval 无
 */
void motor_set_speed_left(motor_handle_t *handle,
                          GPTIMER_Regs *htim,
                          int16_t speed)
{
    if (!handle || !htim) {
        return;
    }

    if (speed >= MOTOR_MIN_SPEED) {
        if (speed > MOTOR_MAX_SPEED) {
            speed = MOTOR_MAX_SPEED;
        }

        if (speed < MOTOR_EFFECTIVE_MIN_SPEED) {
            pwm_set_compare_ch0(htim, 0);
            return;
        }

        DL_GPIO_setPins(handle->port, handle->l_ph_in2_pin);
        pwm_set_compare_ch0(htim, (uint16_t)speed);
    } else {
        int16_t temp = -speed;
        if (temp > MOTOR_MAX_SPEED) {
            temp = MOTOR_MAX_SPEED;
        }

        if (temp < MOTOR_EFFECTIVE_MIN_SPEED) {
            pwm_set_compare_ch0(htim, 0);
            return;
        }

        DL_GPIO_clearPins(handle->port, handle->l_ph_in2_pin);
        pwm_set_compare_ch0(htim, (uint16_t)temp);
    }
}

/**
 * @brief  设置右电机速度
 * @note   方向引脚极性与左电机相反，同时更新 encoder 方向标志位
 * @param  handle  电机句柄指针
 * @param  htim    PWM 定时器句柄
 * @param  speed   PWM 比较值（-PWM_MAX_COMPARE ~ +PWM_MAX_COMPARE）
 * @retval 无
 */
void motor_set_speed_right(motor_handle_t *handle,
                           GPTIMER_Regs *htim,
                           int16_t speed)
{
    if (!handle || !htim) {
        return;
    }

    if (speed >= MOTOR_MIN_SPEED) {
        if (speed > MOTOR_MAX_SPEED) {
            speed = MOTOR_MAX_SPEED;
        }

        if (speed < MOTOR_EFFECTIVE_MIN_SPEED) {
            pwm_set_compare_ch1(htim, 0);
            return;
        }

        motor_right_direction_sign = 1;
        DL_GPIO_clearPins(handle->port, handle->r_ph_in2_pin);
        pwm_set_compare_ch1(htim, (uint16_t)speed);
    } else {
        int16_t temp = -speed;
        if (temp > MOTOR_MAX_SPEED) {
            temp = MOTOR_MAX_SPEED;
        }

        if (temp < MOTOR_EFFECTIVE_MIN_SPEED) {
            pwm_set_compare_ch1(htim, 0);
            return;
        }

        motor_right_direction_sign = -1;
        DL_GPIO_setPins(handle->port, handle->r_ph_in2_pin);
        pwm_set_compare_ch1(htim, (uint16_t)temp);
    }
}

/**
 * @brief  获取右电机方向标志位（供 encoder 模块使用）
 * @param  无
 * @retval 1=正向，-1=反向
 */
int8_t motor_get_right_direction_sign(void)
{
    return motor_right_direction_sign;
}
