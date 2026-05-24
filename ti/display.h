#ifndef USER_DISPLAY_H_
#define USER_DISPLAY_H_

#include "header.h"

#define DISPLAY_LINE_1_Y       0
#define DISPLAY_LINE_2_Y       20
#define DISPLAY_LINE_3_Y       40
#define DISPLAY_LINE_4_Y       60
#define DISPLAY_LINE_5_Y       80
#define DISPLAY_LINE_6_Y       100
#define DISPLAY_LINE_7_Y       120
#define DISPLAY_LINE_8_Y       140
#define DISPLAY_LINE_9_Y       160
#define DISPLAY_LINE_10_Y      180
#define DISPLAY_LINE_11_Y      200
#define DISPLAY_LINE_12_Y      220
#define DISPLAY_LINE_13_Y      240
#define DISPLAY_LINE_14_Y      260
#define DISPLAY_LINE_15_Y      280
#define DISPLAY_LINE_16_Y      300
#define DISPLAY_LINE_17_Y      320
#define DISPLAY_LINE_18_Y      340

// 调试界面刷新标志位 (在主定时器中断里置1，任务循环中清0)
extern volatile uint8_t displayRefreshFlag;

void Display_Task(void);

#endif