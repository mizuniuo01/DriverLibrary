#ifndef MENU_H
#define MENU_H

#include <stdint.h>

/* 菜单输入动作（与按键驱动解耦，由应用层做 key→action 映射） */
typedef enum {
    MENU_ACT_NONE = 0,
    MENU_ACT_CONFIRM,       /* K1 短按：确认 */
    MENU_ACT_CONFIRM_LONG,  /* K1 长按：进入步长模式 */
    MENU_ACT_CANCEL,        /* K2 短按：取消/返回 */
    MENU_ACT_UP,            /* K3 首次按下：上翻/增加 */
    MENU_ACT_UP_REPEAT,     /* K3 连发：持续上翻/持续增加 */
    MENU_ACT_DOWN,          /* K4 首次按下：下翻/减少 */
    MENU_ACT_DOWN_REPEAT,   /* K4 连发：持续下翻/持续减少 */
} menu_action_t;

/* 菜单状态机状态 */
typedef enum {
    MENU_STATE_L1_GROUP,    /* L1：分组列表 */
    MENU_STATE_L2_PARAM,    /* L2：参数列表 */
    MENU_STATE_L3_EDIT,     /* L3：编辑数值 */
    MENU_STATE_L3_STEP,     /* L3：编辑步长 */
} menu_state_t;

/* 单个菜单参数描述（应用层 static const 编译期定义） */
typedef struct {
    const char *name;              /* 参数名，如 "Kp" */
    int32_t (*getter)(void);       /* 读取当前值（已按 decimals 缩放） */
    void (*setter)(int32_t val);   /* 写入新值（已缩放） */
    uint8_t  decimals;             /* 小数位数：0=整数，1=1位，2=2位，3=3位 */
    int32_t  step;                 /* 默认步长（与值同量纲） */
    int32_t  min;                  /* 最小值（已缩放） */
    int32_t  max;                  /* 最大值（已缩放） */
} menu_param_t;

/* 菜单分组描述（应用层 static const 编译期定义） */
typedef struct {
    const char         *name;      /* 分组名，如 "PID-Left" */
    const menu_param_t *params;    /* 参数数组指针 */
    uint8_t             param_count;
} menu_group_t;

/* 菜单运行实例（调用者静态分配） */
typedef struct {
    const menu_group_t *groups;
    uint8_t             group_count;

    menu_state_t        state;
    uint8_t             group_index;
    uint8_t             param_index;
    uint8_t             group_scroll;
    uint8_t             param_scroll;
    const menu_group_t *cur_group;
    const menu_param_t *cur_param;

    int32_t             edit_value;
    int32_t             original_value;
    int32_t             edit_step;
    int32_t             original_step;
    uint8_t             dirty;
} menu_t;

/* 菜单渲染调度标志位（ISR 置1，menu_task 清0，周期 ~30ms） */
extern volatile uint8_t menu_task_flag;

void menu_init(menu_t *menu, const menu_group_t *groups, uint8_t group_count);
void menu_task(menu_t *menu, menu_action_t action);

#endif /* MENU_H */
