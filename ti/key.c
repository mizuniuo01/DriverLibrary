#include "key.h"

static const uint32_t keyPins[] = {
    KEY_KEY1_PIN,
    KEY_KEY2_PIN,
    KEY_KEY3_PIN,
    KEY_KEY4_PIN
};

static uint16_t keyPressTick[sizeof(keyPins) / sizeof(keyPins[0])] = {0};
static volatile Key_Event_e keyEvent = KEY_EVENT_NONE;

static const Key_Event_e keyShortEvent[] = {
    KEY1_SHORT_PRESS,
    KEY2_SHORT_PRESS,
    KEY3_SHORT_PRESS,
    KEY4_SHORT_PRESS
};

/**
 * @brief 按键基准状态机，放置于1ms定时中断中运行
 * @param None
 * @retval None
 */
void Key_Tick_Process(void)
{
    uint32_t pinStates = DL_GPIO_readPins(KEY_PORT,
        KEY_KEY1_PIN | KEY_KEY2_PIN | KEY_KEY3_PIN | KEY_KEY4_PIN);

    uint8_t keyCount = (uint8_t)(sizeof(keyPins) / sizeof(keyPins[0]));

    for (uint8_t index = 0; index < keyCount; index++)
    {
        bool isPressed = ((pinStates & keyPins[index]) == 0);

        if (isPressed)
        {
            if (keyPressTick[index] < UINT16_MAX)
                keyPressTick[index]++;
        }
        else
        {
            if (keyPressTick[index] >= KEY_DEBOUNCE_TIME && keyEvent == KEY_EVENT_NONE)
            {
                keyEvent = keyShortEvent[index];
            }

            keyPressTick[index] = 0;
        }
    }
}

/**
 * @brief 获取当前按键事件并自动清零标志位
 * @param None
 * @retval Key_Event_e 提取到的合法操作事件(短按/长按/无)
 */
Key_Event_e Key_Get_Event(void)
{
    uint32_t primaskBit = __get_PRIMASK();
    __disable_irq();

    Key_Event_e event = keyEvent;
    keyEvent = KEY_EVENT_NONE;

    __set_PRIMASK(primaskBit);
    return event;
}
