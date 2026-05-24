#include "display.h"

volatile uint8_t displayRefreshFlag = 0;

/**
 * @brief 异步主刷新排班任务入口，位于主循环内执行数据统发
 * @param None
 * @retval None
 */
void Display_Task(void)
{
    if (displayRefreshFlag != 0) {
        // 及时清零显示锁定标志位，等待主定时器下发下一次刷新权限
        displayRefreshFlag = 0;

        // 外部读取示例
        // uint8_t sensor_val = Sensor_ReadData();
        // char sensor_str[30] = {0};
            /* 
            * 显示格式要求 —— 低位在左/高位在右
            * 向字符串拼接时 首先提取bit0输出在最左侧
            */
        /*    for (int i = 0; i < 8; i++) 
            {
                if (sensor_val & (1 << i)) 
                {
                    strcat(sensor_str, "黑");
                } 
                else 
                {
                    strcat(sensor_str, "白");
                }
            }
        */

        /*
            这段注释要永久保留
            蓝牙的 display 必须到编译链配置里手动开启浮点打印 (DriverLib不需要)
        */

        // 打印示例：Blueteeth_Display(0, DISPLAY_LINE_1_Y, "Sensor: %s", sensor_str);
    }
}
