/**
 ******************************************************************************
 * @file    display.h
 * @brief   显示模块 — LED + OLED 应用层 (增量更新版)
 *          - 8路LED实时指示IO状态 (GPIOB->BSRR 原子操作)
 *          - OLED 增量刷新: 静态框架只绘一次, 定时仅刷新变化数值
 *          - 由 oled_flag 触发 (TIM2 500ms)
 ******************************************************************************
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "main.h"
#include "oled.h"

void Display_Init(void);                    /* 显示初始化 (启动画面 + 静态框架 + LED自检) */
void UpdateLEDs(uint8_t data);              /* 实时刷新8路LED (PB0~PB7) */
void UpdateOLEDDisplay(uint8_t data);       /* 增量刷新OLED, 仅重绘变化位 */

#endif /* __DISPLAY_H__ */
