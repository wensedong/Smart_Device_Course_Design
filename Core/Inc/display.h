/**
 ******************************************************************************
 * @file    display.h
 * @brief   显示模块 — LED + OLED 应用层
 *          - 8路LED实时指示IO状态
 *          - OLED周期刷新显示采样数据
 ******************************************************************************
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "main.h"
#include "oled.h"

/* ======================== 配置 ======================== */

#define OLED_UPDATE_TICK  5   /* OLED每N次采样刷新 (5×100ms=500ms) */

/* ======================== 函数接口 ======================== */

void Display_Init(void);                    /* 显示初始化 (OLED启动画面) */
void UpdateLEDs(uint8_t data);              /* 刷新8路LED (PB0~PB7) */
void UpdateOLEDDisplay(uint8_t data);       /* 刷新OLED (IO状态+采样信息) */

#endif /* __DISPLAY_H__ */
