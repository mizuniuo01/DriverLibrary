/**
 * @file    menu.c
 * @brief   OLED 按键菜单模块（多级菜单 + 现场调参）
 * @author  mizuniuo01
 * @date    2026-05-28
 * @version 1.0.0
 * @note    依赖：oled.h、key.h、error_handler.h
 * @note    与按键驱动解耦：menu_task 接收抽象 menu_action_t，不依赖 key 句柄，
 *           应用层通过 map_key() 函数做 key_event_t → menu_action_t 映射
 * @note    平台无关：ST 和 TI 共用（前提是 oled API 一致，且 oled_task 已实现非阻塞调度）
 * @note    渲染由 menu_task_flag 节流（ISR 定时置位，建议 ~30ms），
 *           动作处理即时响应不分频；菜单内部已调 oled_clear/oled_update，
 *           使用菜单时应用层不再单独调 oled 相关 task，但需在主循环中保留 oled_task()
 * @warning 此文件需放在含 oled.h 和 error_handler.h 的 include path 中编译
 *
 * ====== 架构说明 ======
 *
 * 三级菜单状态机：
 *   L1_GROUP（分组列表）──K1短按──→ L2_PARAM（参数列表）──K1短按──→ L3_EDIT（编辑数值）
 *       ↑                         ↑  K2短按                          │
 *       └───K2短按─────────────────┘                    K1短按(确认)/K2短按(取消恢复)
 *                                                       ↓
 *                                                   L2_PARAM
 *                                                        ↕ K1长按(进入)/K1短按(确认)/K2短按(取消)
 *                                                   L3_STEP（编辑步长）
 *
 * 按键逻辑映射（由应用层 map_key 实现，不在本文件中）：
 *   K1(确认): SHORT→进入/确认, LONG→L3_EDIT 内进入步长模式
 *   K2(取消): SHORT→返回/取消恢复
 *   K3(上/增): PRESS→首次触发, REPEAT→连发
 *   K4(下/减): PRESS→首次触发, REPEAT→连发
 *
 * 值体系统一为 int32_t + decimals（小数位数），不使用 float：
 *   - val=1500, decimals=3 → 显示 "1.500"
 *   - val=500,  decimals=0 → 显示 "500"
 *   - getter/setter 做 float↔int32 转换（乘/除 10^decimals），由模块提供
 *
 * ====== 文件组织 ======
 *
 * menu.h         — 公共类型（menu_action_t、menu_param_t、menu_group_t、menu_t）+ API
 * menu.c         — 状态机 + 渲染 + 菜单入口（本文件）
 * menu_config.h  — 菜单配置数组（menu_group_t[] 和 menu_param_t[]），
 *                  由 menu.c 末尾 #include，集中管理所有可调参数
 *
 * getter/setter 函数由各模块自己的文件提供（如 encoder_get_left 已在 encoder.c），
 * 或由应用层在对应实例文件中提供 wrapper（如封装 pid_left.kp 的读写）。
 * 函数声明放在模块自己的 .h 或 menu_config.h 中，确保 menu_config.h 中
 * 的数组定义可以引用。
 *
 * ====== 使用步骤 ======
 *
 * ── 步骤 1：为需要调参的模块提供 getter/setter ──
 *
 * // 例：在创建 pid_left 实例的应用层文件中
 * static int32_t app_kp_get(void) { return (int32_t)(pid_left.kp * 1000.0f); }
 * static void    app_kp_set(int32_t v) { pid_left.kp = (float)v / 1000.0f; }
 *
 * // 纯整型参数（如电机速度）不需要缩放
 * static int32_t app_speed_get(void) { return (int32_t)motor_target_speed; }
 * static void    app_speed_set(int32_t v) { motor_target_speed = (int16_t)v; }
 *
 * // 已有 getter 的模块（如 encoder）直接复用，无需额外封装
 * // encoder_get_left() 返回 int16_t，需包一层转为 int32_t
 *
 * ── 步骤 2：在 menu_config.h 中定义菜单结构 ──
 *
 * // menu_config.h（被 menu.c 末尾 #include）
 * static const menu_param_t pid_left_params[] = {
 *     { "Kp", app_kp_get, app_kp_set, 3, 100, 0, 100000 },
 *     { "Ki", app_ki_get, app_ki_set, 3, 10,  0, 10000  },
 * };
 * static const menu_group_t menu_groups[] = {
 *     { "PID-Left",  pid_left_params, 2 },
 * };
 * #define MENU_GROUP_COUNT 1
 *
 * ── 步骤 3：主循环中集成 ──
 *
 * // key→action 映射（在应用层文件中）
 * static menu_action_t map_key(key_handle_t *keys)
 * {
 *     key_event_t e;
 *     e = key_get_event(keys, 0);
 *     if (e == KEY_EVENT_SHORT_PRESS) return MENU_ACT_CONFIRM;
 *     if (e == KEY_EVENT_LONG_PRESS)  return MENU_ACT_CONFIRM_LONG;
 *     e = key_get_event(keys, 1);
 *     if (e == KEY_EVENT_SHORT_PRESS) return MENU_ACT_CANCEL;
 *     e = key_get_event(keys, 2);
 *     if (e == KEY_EVENT_PRESS)       return MENU_ACT_UP;
 *     if (e == KEY_EVENT_REPEAT)      return MENU_ACT_UP_REPEAT;
 *     e = key_get_event(keys, 3);
 *     if (e == KEY_EVENT_PRESS)       return MENU_ACT_DOWN;
 *     if (e == KEY_EVENT_REPEAT)      return MENU_ACT_DOWN_REPEAT;
 *     return MENU_ACT_NONE;
 * }
 *
 * // ISR 中
 * void Timer_IRQHandler(void) { menu_task_flag = 1; }
 *
 * // 主循环
 * menu_t menu;
 * menu_init(&menu, menu_groups, MENU_GROUP_COUNT);
 * while (1) {
 *     key_task(&keys);
 *     menu_task(&menu, map_key(&keys));
 *     oled_task();
 * }
 */

#include "menu.h"
#include "oled.h"
#include "error_handler.h"

volatile uint8_t menu_task_flag;

/* 显示布局常量 */
#define MENU_CONTENT_Y     8   /* 内容区起始 Y */
#define MENU_LINE_HEIGHT   8   /* 行高（6×8 字体） */
#define MENU_VISIBLE_ITEMS 5   /* L1/L2 可见行数 */
#define MENU_HINT_BAR_Y    56  /* 提示栏 Y */
#define MENU_TITLE_X       22  /* "=== MENU ===" 居中 X */

/* L3 步长编辑范围（防止 ×10 溢出 int32_t） */
#define MENU_STEP_MAX 100000000
#define MENU_STEP_MIN 1

/* 各状态提示栏文案 */
#define MENU_HINT_L1      "1:enter  3/4:nav"
#define MENU_HINT_L2      "1:ent  2:bk  3/4:nav"
#define MENU_HINT_L3_EDIT "1:ok  2:cncl  3/4:+/-"
#define MENU_HINT_L3_STEP "1:ok  2:cncl  3/4:step"

/**
 * @brief  将 int32_t 值格式化为带小数点的字符串
 * @note   不做四舍五入，整数部分和分数部分直接截取
 * @param  buf       输出缓冲区
 * @param  val       整数值（已按 decimals 缩放）
 * @param  decimals  小数位数
 * @retval 字符串长度（不含 '\0'）
 */
static uint8_t menu_int32_to_str(char *buf, int32_t val, uint8_t decimals)
{
    uint8_t pos;
    uint8_t is_neg;
    int32_t divisor;
    uint8_t i;
    int32_t int_part;
    int32_t frac_part;
    uint8_t int_start;
    uint8_t j;
    char tmp;

    pos    = 0;
    is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val    = -val;
    }

    divisor = 1;
    for (i = 0; i < decimals; i++) {
        divisor *= 10;
    }

    int_part  = val / divisor;
    frac_part = val % divisor;

    if (is_neg) {
        buf[pos++] = '-';
    }

    /* 整数部分：从右往左写入，再反转 */
    int_start = pos;
    if (int_part == 0) {
        buf[pos++] = '0';
    } else {
        while (int_part > 0) {
            buf[pos++]   = '0' + (int_part % 10);
            int_part    /= 10;
        }
        /* 反转整数位 */
        for (i = 0; i < (pos - int_start) / 2; i++) {
            tmp                       = buf[int_start + i];
            buf[int_start + i]        = buf[pos - 1 - i];
            buf[pos - 1 - i]          = tmp;
        }
    }

    /* 小数部分 */
    if (decimals > 0) {
        buf[pos++] = '.';
        for (i = 0; i < decimals; i++) {
            divisor    /= 10;
            buf[pos++]  = '0' + (uint8_t)((frac_part / divisor) % 10);
        }
    }

    buf[pos] = '\0';
    return pos;
}

/**
 * @brief  调整滚动窗口，使选中项可见
 * @param  index   当前选中索引
 * @param  scroll  滚动窗口起始索引（输入输出）
 * @param  count   总数量
 * @retval 无
 */
static void menu_adjust_scroll(uint8_t index, uint8_t *scroll, uint8_t count)
{
    if (count <= MENU_VISIBLE_ITEMS) {
        *scroll = 0;
        return;
    }

    if (index < *scroll) {
        *scroll = index;
    } else if (index >= *scroll + MENU_VISIBLE_ITEMS) {
        *scroll = index - MENU_VISIBLE_ITEMS + 1;
    }
}

/**
 * @brief  渲染 L1：分组列表
 * @param  menu  菜单实例指针
 * @retval 无
 */
static void menu_render_l1(const menu_t *menu)
{
    uint8_t i;
    uint8_t idx;
    uint8_t x;

    oled_show_string(MENU_TITLE_X, 0, "=== MENU ===", OLED_FONT_6X8);

    for (i = 0; i < MENU_VISIBLE_ITEMS; i++) {
        idx = menu->group_scroll + i;
        if (idx >= menu->group_count) {
            break;
        }

        x = 0;
        if (idx == menu->group_index) {
            oled_show_char(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT, '>',
                OLED_FONT_6X8);
        }
        x += OLED_FONT_6X8;
        oled_show_string(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT,
            menu->groups[idx].name, OLED_FONT_6X8);
    }

    oled_show_string(0, MENU_HINT_BAR_Y, MENU_HINT_L1, OLED_FONT_6X8);
}

/**
 * @brief  渲染 L2：参数列表
 * @param  menu  菜单实例指针
 * @retval 无
 */
static void menu_render_l2(const menu_t *menu)
{
    uint8_t i;
    uint8_t idx;
    uint8_t x;
    const menu_param_t *param;
    int32_t val;
    char buf[22];
    uint8_t len;

    /* 面包屑：组名 + ">" */
    oled_show_string(0, 0, menu->cur_group->name, OLED_FONT_6X8);
    x = OLED_FONT_6X8;
    len = 0;
    while (menu->cur_group->name[len] != '\0') {
        len++;
    }
    x = OLED_FONT_6X8 * len;
    oled_show_char(x, 0, '>', OLED_FONT_6X8);

    for (i = 0; i < MENU_VISIBLE_ITEMS; i++) {
        idx = menu->param_scroll + i;
        if (idx >= menu->cur_group->param_count) {
            break;
        }

        param = &menu->cur_group->params[idx];
        val   = param->getter();

        /* ">Name: value" 逐段绘制 */
        x = 0;
        if (idx == menu->param_index) {
            oled_show_char(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT, '>',
                OLED_FONT_6X8);
        }
        x += OLED_FONT_6X8;

        oled_show_string(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT,
            param->name, OLED_FONT_6X8);
        len = 0;
        while (param->name[len] != '\0') {
            len++;
        }
        x += OLED_FONT_6X8 * len;

        oled_show_string(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT, ": ",
            OLED_FONT_6X8);
        x += OLED_FONT_6X8 * 2;

        len = menu_int32_to_str(buf, val, param->decimals);
        oled_show_string(x, MENU_CONTENT_Y + i * MENU_LINE_HEIGHT, buf,
            OLED_FONT_6X8);
    }

    oled_show_string(0, MENU_HINT_BAR_Y, MENU_HINT_L2, OLED_FONT_6X8);
}

/**
 * @brief  构建面包屑字符串 "Group>Param"
 * @param  buf      输出缓冲区
 * @param  buf_size 缓冲区大小
 * @param  group    组名
 * @param  param    参数名
 * @retval 字符串长度
 */
static uint8_t menu_fmt_breadcrumb(char *buf, uint8_t buf_size,
    const char *group, const char *param)
{
    uint8_t pos;
    uint8_t i;

    pos = 0;

    i = 0;
    while (group[i] != '\0' && pos < buf_size - 1) {
        buf[pos++] = group[i++];
    }

    if (pos < buf_size - 1) {
        buf[pos++] = '>';
    }

    i = 0;
    while (param[i] != '\0' && pos < buf_size - 1) {
        buf[pos++] = param[i++];
    }

    buf[pos] = '\0';
    return pos;
}

/**
 * @brief  渲染 L3_EDIT：编辑数值
 * @param  menu  菜单实例指针
 * @retval 无
 */
static void menu_render_l3_edit(const menu_t *menu)
{
    char buf[22];
    uint8_t len;
    uint8_t x;
    uint8_t pos;

    /* 面包屑 */
    menu_fmt_breadcrumb(buf, sizeof(buf), menu->cur_group->name,
        menu->cur_param->name);
    oled_show_string(0, 0, buf, OLED_FONT_6X8);

    /* 当前值（8×16 大字体，居中） */
    len = menu_int32_to_str(buf, menu->edit_value, menu->cur_param->decimals);
    x   = (OLED_WIDTH - len * OLED_FONT_8X16) / 2;
    oled_show_string(x, 16, buf, OLED_FONT_8X16);

    /* 步长 */
    oled_show_string(0, 32, "Step: ", OLED_FONT_6X8);
    pos = menu_int32_to_str(buf, menu->edit_step, menu->cur_param->decimals);
    oled_show_string(6 * OLED_FONT_6X8, 32, buf, OLED_FONT_6X8);

    /* 范围：min~max */
    pos = menu_int32_to_str(buf, menu->cur_param->min, menu->cur_param->decimals);
    buf[pos++] = '~';
    menu_int32_to_str(&buf[pos], menu->cur_param->max,
        menu->cur_param->decimals);
    oled_show_string(0, 40, buf, OLED_FONT_6X8);

    oled_show_string(0, MENU_HINT_BAR_Y, MENU_HINT_L3_EDIT, OLED_FONT_6X8);
}

/**
 * @brief  渲染 L3_STEP：编辑步长（步长行有 '>' 光标）
 * @param  menu  菜单实例指针
 * @retval 无
 */
static void menu_render_l3_step(const menu_t *menu)
{
    char buf[22];
    uint8_t len;
    uint8_t x;
    uint8_t pos;

    /* 面包屑 */
    menu_fmt_breadcrumb(buf, sizeof(buf), menu->cur_group->name,
        menu->cur_param->name);
    oled_show_string(0, 0, buf, OLED_FONT_6X8);

    /* 当前值（8×16 大字体，居中） */
    len = menu_int32_to_str(buf, menu->edit_value, menu->cur_param->decimals);
    x   = (OLED_WIDTH - len * OLED_FONT_8X16) / 2;
    oled_show_string(x, 16, buf, OLED_FONT_8X16);

    /* 步长（带 '>' 光标） */
    oled_show_string(0, 32, ">Step: ", OLED_FONT_6X8);
    pos = menu_int32_to_str(buf, menu->edit_step, menu->cur_param->decimals);
    oled_show_string(7 * OLED_FONT_6X8, 32, buf, OLED_FONT_6X8);

    /* 范围：min~max */
    pos = menu_int32_to_str(buf, menu->cur_param->min, menu->cur_param->decimals);
    buf[pos++] = '~';
    menu_int32_to_str(&buf[pos], menu->cur_param->max,
        menu->cur_param->decimals);
    oled_show_string(0, 40, buf, OLED_FONT_6X8);

    oled_show_string(0, MENU_HINT_BAR_Y, MENU_HINT_L3_STEP, OLED_FONT_6X8);
}

/* ===== 各状态按键处理 ===== */

/**
 * @brief  处理 L1_GROUP 状态下的按键动作
 * @param  menu    菜单实例指针
 * @param  action  菜单动作
 * @retval 无
 */
static void menu_handle_l1(menu_t *menu, menu_action_t action)
{
    switch (action) {
    case MENU_ACT_CONFIRM:
        if (menu->group_count == 0) {
            break;
        }
        menu->cur_group   = &menu->groups[menu->group_index];
        menu->param_index  = 0;
        menu->param_scroll = 0;
        menu->state        = MENU_STATE_L2_PARAM;
        menu->dirty        = 1;
        break;

    case MENU_ACT_UP:
    case MENU_ACT_UP_REPEAT:
        if (menu->group_index > 0) {
            menu->group_index--;
            menu_adjust_scroll(menu->group_index, &menu->group_scroll,
                menu->group_count);
            menu->dirty = 1;
        }
        break;

    case MENU_ACT_DOWN:
    case MENU_ACT_DOWN_REPEAT:
        if (menu->group_index + 1 < menu->group_count) {
            menu->group_index++;
            menu_adjust_scroll(menu->group_index, &menu->group_scroll,
                menu->group_count);
            menu->dirty = 1;
        }
        break;

    default:
        break;
    }
}

/**
 * @brief  处理 L2_PARAM 状态下的按键动作
 * @param  menu    菜单实例指针
 * @param  action  菜单动作
 * @retval 无
 */
static void menu_handle_l2(menu_t *menu, menu_action_t action)
{
    switch (action) {
    case MENU_ACT_CONFIRM:
        if (!menu->cur_group || menu->cur_group->param_count == 0) {
            break;
        }
        menu->cur_param      = &menu->cur_group->params[menu->param_index];
        menu->edit_value      = menu->cur_param->getter();
        menu->original_value  = menu->edit_value;
        menu->edit_step       = menu->cur_param->step;
        menu->original_step   = menu->edit_step;
        menu->state           = MENU_STATE_L3_EDIT;
        menu->dirty           = 1;
        break;

    case MENU_ACT_CANCEL:
        menu->cur_group   = NULL;
        menu->cur_param   = NULL;
        menu->state       = MENU_STATE_L1_GROUP;
        menu->dirty       = 1;
        break;

    case MENU_ACT_UP:
    case MENU_ACT_UP_REPEAT:
        if (!menu->cur_group) {
            break;
        }
        if (menu->param_index > 0) {
            menu->param_index--;
            menu_adjust_scroll(menu->param_index, &menu->param_scroll,
                menu->cur_group->param_count);
            menu->dirty = 1;
        }
        break;

    case MENU_ACT_DOWN:
    case MENU_ACT_DOWN_REPEAT:
        if (!menu->cur_group) {
            break;
        }
        if (menu->param_index + 1 < menu->cur_group->param_count) {
            menu->param_index++;
            menu_adjust_scroll(menu->param_index, &menu->param_scroll,
                menu->cur_group->param_count);
            menu->dirty = 1;
        }
        break;

    default:
        break;
    }
}

/**
 * @brief  处理 L3_EDIT 状态下的按键动作
 * @param  menu    菜单实例指针
 * @param  action  菜单动作
 * @retval 无
 */
static void menu_handle_l3_edit(menu_t *menu, menu_action_t action)
{
    if (!menu->cur_param) {
        return;
    }

    switch (action) {
    case MENU_ACT_CONFIRM:
        menu->cur_param->setter(menu->edit_value);
        menu->state = MENU_STATE_L2_PARAM;
        menu->dirty = 1;
        break;

    case MENU_ACT_CONFIRM_LONG:
        menu->original_step = menu->edit_step;
        menu->state         = MENU_STATE_L3_STEP;
        menu->dirty         = 1;
        break;

    case MENU_ACT_CANCEL:
        menu->cur_param->setter(menu->original_value);
        menu->state = MENU_STATE_L2_PARAM;
        menu->dirty = 1;
        break;

    case MENU_ACT_UP:
    case MENU_ACT_UP_REPEAT:
        menu->edit_value += menu->edit_step;
        if (menu->edit_value > menu->cur_param->max) {
            menu->edit_value = menu->cur_param->max;
        }
        menu->dirty = 1;
        break;

    case MENU_ACT_DOWN:
    case MENU_ACT_DOWN_REPEAT:
        menu->edit_value -= menu->edit_step;
        if (menu->edit_value < menu->cur_param->min) {
            menu->edit_value = menu->cur_param->min;
        }
        menu->dirty = 1;
        break;

    default:
        break;
    }
}

/**
 * @brief  处理 L3_STEP 状态下的按键动作
 * @param  menu    菜单实例指针
 * @param  action  菜单动作
 * @retval 无
 */
static void menu_handle_l3_step(menu_t *menu, menu_action_t action)
{
    switch (action) {
    case MENU_ACT_CONFIRM:
        menu->state = MENU_STATE_L3_EDIT;
        menu->dirty = 1;
        break;

    case MENU_ACT_CANCEL:
        menu->edit_step = menu->original_step;
        menu->state     = MENU_STATE_L3_EDIT;
        menu->dirty     = 1;
        break;

    case MENU_ACT_UP:
        if (menu->edit_step <= MENU_STEP_MAX / 10) {
            menu->edit_step *= 10;
            menu->dirty      = 1;
        }
        break;

    case MENU_ACT_DOWN:
        if (menu->edit_step >= MENU_STEP_MIN * 10) {
            menu->edit_step /= 10;
            menu->dirty      = 1;
        }
        break;

    default:
        break;
    }
}

/* ===== 公共接口 ===== */

/**
 * @brief  菜单模块初始化
 * @param  menu         菜单实例指针
 * @param  groups       分组数组指针
 * @param  group_count  分组数量
 * @retval 无
 */
void menu_init(menu_t *menu, const menu_group_t *groups, uint8_t group_count)
{
    if (!menu || !groups || group_count == 0) {
        error_report(ERROR_SOURCE_MENU, DRV_ERR_PARAM);
        return;
    }

    menu->groups       = groups;
    menu->group_count  = group_count;
    menu->state        = MENU_STATE_L1_GROUP;
    menu->group_index  = 0;
    menu->param_index  = 0;
    menu->group_scroll = 0;
    menu->param_scroll = 0;
    menu->cur_group    = NULL;
    menu->cur_param    = NULL;
    menu->edit_value   = 0;
    menu->original_value = 0;
    menu->edit_step    = 0;
    menu->original_step = 0;
    menu->dirty        = 1;
}

/**
 * @brief  菜单任务（主循环中调用，每次调用处理一个 menu_action_t）
 * @param  menu    菜单实例指针
 * @param  action  菜单动作（MENU_ACT_NONE 表示无动作）
 * @retval 无
 */
void menu_task(menu_t *menu, menu_action_t action)
{
    if (!menu) {
        error_report(ERROR_SOURCE_MENU, DRV_ERR_PARAM);
        return;
    }

    /* 动作处理：即时响应，不依赖 flag */
    if (action != MENU_ACT_NONE) {
        switch (menu->state) {
        case MENU_STATE_L1_GROUP:
            menu_handle_l1(menu, action);
            break;
        case MENU_STATE_L2_PARAM:
            menu_handle_l2(menu, action);
            break;
        case MENU_STATE_L3_EDIT:
            menu_handle_l3_edit(menu, action);
            break;
        case MENU_STATE_L3_STEP:
            menu_handle_l3_step(menu, action);
            break;
        }
    }

    /* 渲染：由 menu_task_flag 节流（ISR 定时置位） */
    if (!menu_task_flag) {
        return;
    }
    menu_task_flag = 0;

    if (!menu->dirty) {
        return;
    }

    oled_clear();
    switch (menu->state) {
    case MENU_STATE_L1_GROUP:
        menu_render_l1(menu);
        break;
    case MENU_STATE_L2_PARAM:
        menu_render_l2(menu);
        break;
    case MENU_STATE_L3_EDIT:
        menu_render_l3_edit(menu);
        break;
    case MENU_STATE_L3_STEP:
        menu_render_l3_step(menu);
        break;
    }
    oled_update();
    menu->dirty = 0;
}

/*
 * 菜单配置集中管理：menu_param_t[] 和 menu_group_t[] 定义在 menu_config.h 中，
 * 由本文件末尾 include。新增可调参数只需编辑 menu_config.h，无需修改本文件。
 */
#include "menu_config.h"
