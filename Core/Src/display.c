/**
 ******************************************************************************
 * @file    display.c
 * @brief   显示模块实现
 *          - UpdateLEDs():  直接写GPIOB->BSRR原子刷新8路LED
 *          - UpdateOLEDDisplay(): 帧缓冲模式更新OLED
 *          - Display_Init(): 上电启动画面
 ******************************************************************************
 */

#include "display.h"
#include "daq.h"
#include <stdio.h>

static char disp_buf[22];  /* OLED字符串缓冲 (21字符/行 + null) */

/* ======================== 初始化 ======================== */

/**
 * @brief  显示启动画面 (OLED + LED自检)
 */
void Display_Init(void)
{
    /* OLED启动画面 */
    OLED_Init();
    OLED_ShowStr(0, 0, (unsigned char *)"  Remote DAQ", 1);
    OLED_ShowStr(0, 1, (unsigned char *)" STM32F407ZGT6", 1);
    OLED_ShowStr(0, 3, (unsigned char *)"  System Ready", 1);
    HAL_Delay(1500);
    OLED_Clear_Gram();
    OLED_Refresh_Gram();

    /* LED全亮自检 */
    GPIOB->BSRR = 0x00FF;           /* PB0~PB7 全部点亮 */
    HAL_Delay(300);
    GPIOB->BSRR = 0x00FF << 16;     /* PB0~PB7 全部熄灭 */
}

/* ======================== LED刷新 ======================== */

/**
 * @brief  刷新8路LED (PB0~PB7), 与IO状态一一对应
 * @param  data : bit0=LED0 ... bit7=LED7  (1=亮 0=灭)
 * @note   使用BSRR寄存器实现原子操作:
 *         BSRR[15:0] = 置位 (输出高), BSRR[31:16] = 复位 (输出低)
 */
void UpdateLEDs(uint8_t data)
{
    uint16_t set_bits =  data & 0x00FF;         /* IO=1 → LED亮 */
    uint16_t rst_bits = (~data) & 0x00FF;       /* IO=0 → LED灭 */
    GPIOB->BSRR = set_bits | (rst_bits << 16);
}

/* ======================== OLED刷新 ======================== */

/**
 * @brief  帧缓冲方式刷新OLED
 *         显示内容:
 *           第0行: 标题 "== Remote DAQ =="
 *           第1~4行: 8路IO状态 (CH0~7)
 *           第5行: 采样计数
 *           第7行: 采样率
 * @param  data : 当前8路IO状态
 */
void UpdateOLEDDisplay(uint8_t data)
{
    uint8_t i;

    OLED_Clear_Gram();

    /* 标题栏 */
    OLED_ShowStr(0, 0, (unsigned char *)"== Remote DAQ ==", 1);

    /* 8路IO状态: 左半屏 CH0~3, 右半屏 CH4~7 */
    for (i = 0; i < 8; i++) {
        sprintf(disp_buf, "CH%d:%d", i, (data >> i) & 1);
        OLED_ShowStr(i < 4 ? 0 : 64, (i % 4) + 1,
                     (unsigned char *)disp_buf, 1);
    }

    /* 采样计数 */
    sprintf(disp_buf, "Sample:%lu", sample_count);
    OLED_ShowStr(0, 5, (unsigned char *)disp_buf, 1);

    /* 采样率 */
    OLED_ShowStr(0, 7, (unsigned char *)"Rate:10Hz 100ms", 1);

    OLED_Refresh_Gram();
}
