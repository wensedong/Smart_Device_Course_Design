/**
 ******************************************************************************
 * @file    display.c
 * @brief   显示模块实现
 *          - UpdateLEDs():  直接写GPIOB->BSRR原子刷新8路LED
 *          - UpdateOLEDDisplay(): 帧缓冲方式刷新OLED
 *          - Display_Init(): 上电启动画面 + LED自检
 ******************************************************************************
 */

#include "display.h"
#include "daq.h"
#include <stdio.h>

static char disp_buf[22];  /* OLED字符串缓冲 */

/* ======================== 初始化 ======================== */

void Display_Init(void)
{
    OLED_Init();

    /* 启动画面 */
    OLED_ShowStr(0, 0, (unsigned char *)"  Remote DAQ", 1);
    OLED_ShowStr(0, 1, (unsigned char *)" STM32F407ZGT6", 1);
    OLED_ShowStr(0, 2, (unsigned char *)" 8EXIT + 2KEY",  1);
    OLED_ShowStr(0, 4, (unsigned char *)"  System Ready", 1);
    HAL_Delay(1500);
    OLED_Clear_Gram();
    OLED_Refresh_Gram();

    /* LED全亮自检 */
    GPIOB->BSRR = 0x00FF;
    HAL_Delay(300);
    GPIOB->BSRR = 0x00FF << 16;
}

/* ======================== LED刷新 ======================== */

/**
 * @brief  实时刷新8路LED (PB0~PB7)
 *         IO=1 → LED亮, IO=0 → LED灭
 */
void UpdateLEDs(uint8_t data)
{
    uint16_t set_bits =  data & 0x00FF;
    uint16_t rst_bits = (~data) & 0x00FF;
    GPIOB->BSRR = set_bits | (rst_bits << 16);
}

/* ======================== OLED刷新 ======================== */

void UpdateOLEDDisplay(uint8_t data)
{
    uint8_t i;

    OLED_Clear_Gram();

    /* 标题 + 模式 */
    OLED_ShowStr(0, 0, (unsigned char *)"== Remote DAQ ==", 1);

    /* 8路IO状态: 左半屏 CH0~3, 右半屏 CH4~7 */
    for (i = 0; i < 8; i++) {
        sprintf(disp_buf, "CH%d:%d", i, (data >> i) & 1);
        OLED_ShowStr(i < 4 ? 0 : 64, (i % 4) + 1,
                     (unsigned char *)disp_buf, 1);
    }

    /* 模式 + 发送计数 */
    sprintf(disp_buf, "%s  S:%lu",
            mode == MODE_AUTO ? "AUTO" : "MANU",
            sample_count);
    OLED_ShowStr(0, 5, (unsigned char *)disp_buf, 1);

    /* 状态栏 */
    OLED_ShowStr(0, 7, (unsigned char *)"Rate:100ms  Exti", 1);

    OLED_Refresh_Gram();
}
