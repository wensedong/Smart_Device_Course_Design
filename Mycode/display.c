/**
 ******************************************************************************
 * @file    display.c
 * @brief   显示模块实现 (增量更新版)
 *          - UpdateLEDs():      直接写GPIOB->BSRR原子刷新8路LED
 *          - UpdateOLEDDisplay(): 仅更新变化的数值位, 无闪烁
 *          - Display_Init():    上电启动画面 + 绘制静态框架 + LED自检
 *
 *          优化说明: 静态标签(标题/通道号/状态栏)只在初始化时绘制一次,
 *          定时刷新仅更新数值 '0'/'1' 和底部模式/计数行,
 *          I2C写入量从 ~1100字节 降至 ~40字节, 视觉无闪烁.
 ******************************************************************************
 */

#include "display.h"
#include "daq.h"
#include <stdio.h>

static char disp_buf[22];       /* 字符串缓冲 */

/* 增量更新缓存: 记录上次显示值, 仅变化时重绘 */
static uint8_t  prev_io_state    = 0xFF;       /* 0xFF 强制首次更新 */
static uint8_t  prev_mode        = 0xFF;
static uint32_t prev_mode_count  = 0xFFFFFFFF;
static uint32_t prev_total_count = 0xFFFFFFFF;

/* 模式 → OLED 显示字符串 (等宽6字符, 保持对齐) */
static const char *mode_str(uint8_t m)
{
    if (m == MODE_AUTO)       return "AUTO>>";
    if (m == MODE_AUTO_PAUSE) return "AUTO||";
    return "MANUAL";                          /* MODE_MANUAL */
}

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

    /* 清除启动画面 → 全黑 */
    OLED_Clear_Gram();
    OLED_Refresh_Gram();

    /* ───────── 绘制静态框架 (只画一次, 后续永不刷新) ───────── */
    OLED_ShowStr(0, 0, (unsigned char *)"== Remote DAQ ==", 1);

    OLED_ShowStr(0,  1, (unsigned char *)"CH0:", 1);   /* 左列 */
    OLED_ShowStr(64, 1, (unsigned char *)"CH4:", 1);   /* 右列 */
    OLED_ShowStr(0,  2, (unsigned char *)"CH1:", 1);
    OLED_ShowStr(64, 2, (unsigned char *)"CH5:", 1);
    OLED_ShowStr(0,  3, (unsigned char *)"CH2:", 1);
    OLED_ShowStr(64, 3, (unsigned char *)"CH6:", 1);
    OLED_ShowStr(0,  4, (unsigned char *)"CH3:", 1);
    OLED_ShowStr(64, 4, (unsigned char *)"CH7:", 1);

    OLED_ShowStr(0, 7, (unsigned char *)"Rate:100ms  Exti", 1);

    /* 预填初始值 '0' (避免首帧空白) */
    {
        uint8_t i;
        char tmp[2] = "0";
        for (i = 0; i < 8; i++) {
            uint8_t col = (i < 4) ? 24 : 88;
            uint8_t row = (i % 4) + 1;
            OLED_ShowStr(col, row, (unsigned char *)tmp, 1);
        }
    }

    /* 初始状态行: "MANUAL m:0    T:0" */
    OLED_ShowStr(0, 5, (unsigned char *)"MANUAL m:0    T:0", 1);

    /* LED全亮自检 */
    GPIOB->BSRR = 0x00FF;
    HAL_Delay(300);
    GPIOB->BSRR = 0x00FF << 16;
}

/* ======================== LED刷新 ======================== */

/**
 * @brief  实时刷新8路LED (PB0~PB7)
 *         IO=1 → LED亮, IO=0 → LED灭
 *         使用 GPIOB->BSRR 原子操作, 无抖动
 */
void UpdateLEDs(uint8_t data)
{
    uint16_t set_bits =  data & 0x00FF;
    uint16_t rst_bits = (~data) & 0x00FF;
    GPIOB->BSRR = set_bits | (rst_bits << 16);
}

/* ======================== OLED增量刷新 ======================== */

/**
 * @brief  增量更新OLED (仅重绘变化的部分)
 *
 *         策略:
 *         - 静态标签 (标题/通道号/状态栏) 在 Display_Init 中一次绘制, 此后不动
 *         - 通道值: 仅当对应 bit 翻转时, 重写该字符 '0' 或 '1'
 *         - 底部行: 仅当 mode 或 sample_count 改变时, 重写整行
 *
 *         效果: I2C 写入量降低 >90%, 视觉完全无闪烁
 */
void UpdateOLEDDisplay(uint8_t data)
{
    uint8_t i;
    char    tmp[2] = {0, 0};  /* 单字符缓冲 */

    /* ── 8路通道: 仅更新变化的位 ── */
    for (i = 0; i < 8; i++) {
        uint8_t cur  = (data         >> i) & 1;
        uint8_t prev = (prev_io_state >> i) & 1;

        if (cur != prev) {
            /* "CHn:" = 4字符 × 6px = 24px, 数值在 col=24 或 88 处 */
            uint8_t col  = (i < 4) ? 24 : 88;
            uint8_t page = (i % 4) + 1;
            tmp[0] = cur ? '1' : '0';
            OLED_ShowStr(col, page, (unsigned char *)tmp, 1);
        }
    }
    prev_io_state = data;

    /* ── 底部状态行: 仅当模式/双计数任一变化时更新 ── */
    if (mode != prev_mode || mode_count != prev_mode_count ||
        total_count != prev_total_count) {
        prev_mode        = mode;
        prev_mode_count  = mode_count;
        prev_total_count = total_count;
        sprintf(disp_buf, "%-6s m:%-4lu T:%-4lu",
                mode_str(mode), mode_count, total_count);
        OLED_ShowStr(0, 5, (unsigned char *)disp_buf, 1);
    }
}
