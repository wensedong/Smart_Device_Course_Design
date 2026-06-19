/**
 ******************************************************************************
 * @file    display.h
 * @brief   显示模块 — LED + OLED 应用层
 *          - 8路LED实时指示IO状态
 *          - OLED周期刷新显示采样数据 (由 oled_flag 触发)
 ******************************************************************************
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "main.h"
#include "oled.h"

void Display_Init(void);                    /* 显示初始化 (OLED启动画面 + LED自检) */
void UpdateLEDs(uint8_t data);              /* 刷新8路LED (PB0~PB7) */
void UpdateOLEDDisplay(uint8_t data);       /* 刷新OLED (IO状态+采样信息) */

#endif /* __DISPLAY_H__ */
