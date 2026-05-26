#ifndef LASER_H
#define LASER_H

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "drv_err.h"

/* 激光配置结构体 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} laser_cfg_t;

/* 激光句柄 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} laser_handle_t;

drv_err_t laser_init(laser_handle_t *handle, const laser_cfg_t *cfg);
void laser_on(laser_handle_t *handle);
void laser_off(laser_handle_t *handle);
void laser_toggle(laser_handle_t *handle);

#endif
