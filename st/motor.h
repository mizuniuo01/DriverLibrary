#ifndef MOTOR_H
#define MOTOR_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

/*
 * 速度参数基于 PWM_MAX_COMPARE=8400（CubeMX 定时器 ARR 值）：
 *   MOTOR_MAX_SPEED = PWM_MAX_COMPARE（100% 占空比）
 *   MOTOR_EFFECTIVE_MIN_SPEED = 252（3% 占空比，死区阈值）
 *   PWM_MAX_COMPARE 变更时需同步调整 MOTOR_MAX_SPEED。
 */
#define MOTOR_MAX_SPEED 8400 /* = PWM_MAX_COMPARE */
#define MOTOR_MIN_SPEED 0 /* 零速 */
#define MOTOR_EFFECTIVE_MIN_SPEED 252 /* 死区阈值，低于此值按 0 输出 */

/* 电机配置结构体 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t l_nsleep_pin;
    uint16_t r_nsleep_pin;
    uint16_t l_ph_in2_pin;
    uint16_t r_ph_in2_pin;
} MotorCfg_t;

/* 电机句柄 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t l_nsleep_pin;
    uint16_t r_nsleep_pin;
    uint16_t l_ph_in2_pin;
    uint16_t r_ph_in2_pin;
} MotorHandle_t;

void motor_init(MotorHandle_t *handle, const MotorCfg_t *cfg);
void motor_set_speed_left(MotorHandle_t *handle, TIM_HandleTypeDef *htim,
                          int16_t speed);
void motor_set_speed_right(MotorHandle_t *handle, TIM_HandleTypeDef *htim,
                           int16_t speed);

#endif
