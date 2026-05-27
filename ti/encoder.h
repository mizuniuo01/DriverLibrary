#ifndef ENCODER_H
#define ENCODER_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 右轮捕获中断计数（ISR 写入，encoder 模块读取） */
extern volatile uint16_t encoder_capture_right_count;

void encoder_init(GPTIMER_Regs *htim_left_qei, GPTIMER_Regs *htim_right_capture);
void encoder_scan_left(GPTIMER_Regs *htim);
void encoder_scan_right(void);
int16_t encoder_get_left(void);
int16_t encoder_get_right(void);

#endif
