#ifndef __KEY_H_
#define __KEY_H_

#include "header.h"

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY1_SHORT_PRESS,
    KEY2_SHORT_PRESS,
    KEY3_SHORT_PRESS,
    KEY4_SHORT_PRESS
} Key_Event_e;

#define KEY_DEBOUNCE_TIME    20  // 按键消抖时间(ms)

void Key_Tick_Process(void);
Key_Event_e Key_Get_Event(void);

#endif