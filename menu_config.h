/**
 * @file    menu_config.h
 * @brief   菜单参数配置（集中管理所有可调参数）
 * @note    此文件被 menu.c 末尾 #include，是项目中唯一需要编辑的菜单文件
 * @note    新增/修改参数只需编辑此文件，无需改动 menu.h 或 menu.c
 * @note    getter/setter 函数声明需在各自的模块头文件或本文件中提前声明
 *
 * ====== 参数定义格式 ======
 *
 * menu_param_t 各字段说明：
 *   name      — 参数名，建议 ≤8 字符，如 "Kp"、"Speed"
 *   getter    — 返回值读取函数，签名 int32_t (*)(void)，返回已缩放的值
 *   setter    — 值写入函数，签名 void (*)(int32_t)，参数为已缩放的值
 *   decimals  — 小数位数：0=整数，1=1位，2=2位，3=3位
 *   step      — 默认步长（与值同量纲），如 100 → 显示 "0.100"（decimals=3 时）
 *   min       — 最小值（已缩放）
 *   max       — 最大值（已缩放）
 *
 * ====== 值缩放说明 ======
 *
 * int32_t 统一值类型，通过 decimals 控制显示：
 *   - 整型参数（decimals=0）: val=500 → 显示 "500"
 *   - 浮点参数（decimals=3）: val=1500 → 显示 "1.500"
 * getter/setter 内部负责 float↔int32 转换：
 *   - int32_t get(void) { return (int32_t)(float_val * 1000.0f); }
 *   - void set(int32_t v) { float_val = (float)v / 1000.0f; }
 */

#if 0
/* ===== 示例：PID 参数配置 ===== */

/*
 * 步骤 1：在创建 pid 实例的应用层文件中提供 getter/setter
 *
 *   static pid_t pid_left;
 *   int32_t app_kp_get(void) { return (int32_t)(pid_left.kp * 1000.0f); }
 *   void    app_kp_set(int32_t v) { pid_left.kp = (float)v / 1000.0f; }
 */

/* 步骤 2：在下方定义参数数组 */

static const menu_param_t pid_left_params[] = {
    { "Kp",   app_kp_get,   app_kp_set,   3, 100, 0,      100000 },
    { "Ki",   app_ki_get,   app_ki_set,   3, 10,  0,      10000  },
    { "Kd",   app_kd_get,   app_kd_set,   3, 10,  0,      10000  },
    { "Imax", app_imax_get, app_imax_set, 0, 100, 0,      100000 },
};

static const menu_param_t pid_right_params[] = {
    { "Kp", app_kp_r_get, app_kp_r_set, 3, 100, 0, 100000 },
    { "Ki", app_ki_r_get, app_ki_r_set, 3, 10,  0, 10000  },
};

static const menu_param_t motor_params[] = {
    /*
     * 纯整型参数示例（decimals=0），无需 float 转换
     * int32_t app_speed_get(void) { return (int32_t)motor_target_speed; }
     * void    app_speed_set(int32_t v) { motor_target_speed = (int16_t)v; }
     */
    /* { "Speed", app_speed_get, app_speed_set, 0, 100, 0, 8400 }, */
};

/* 步骤 3：定义分组数组 */
static const menu_group_t menu_groups[] = {
    { "PID-Left",  pid_left_params,
      sizeof(pid_left_params) / sizeof(pid_left_params[0]) },
    { "PID-Right", pid_right_params,
      sizeof(pid_right_params) / sizeof(pid_right_params[0]) },
    { "Motor",     motor_params,
      sizeof(motor_params) / sizeof(motor_params[0]) },
};

#define MENU_GROUP_COUNT (sizeof(menu_groups) / sizeof(menu_groups[0]))

#endif /* 0 — 使用时删除此行和对应的 #endif */
