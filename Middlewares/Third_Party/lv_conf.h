/**
 * @file lv_conf.h
 * LVGL 8.3.x configuration for the N10 lever board (GC9A01 240x240 round LCD).
 * Enabled via -DLV_CONF_INCLUDE_SIMPLE ; found on the include path.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
/* Keep native order (SquareLine assets are exported for SWAP=0). The
 * GC9A01 wants big-endian RGB565, so the flush callback byte-swaps
 * each pixel before streaming it to SPI. */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/
/* LVGL's own heap (objects, styles, image cache). The UI is light, but keep
 * headroom for SquareLine screens. STM32U585 has 768 KB main RAM. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)
#define LV_MEM_ADR 0

/*====================
   HAL SETTINGS
 *====================*/
/* Drive LVGL's tick from the HAL 1 kHz system tick — no extra timer needed. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "cmsis_os2.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (osKernelGetTickCount())

#define LV_DPI_DEF 130

/* Display refresh + input read cadence. Default is 30 ms (~33 fps). Lower it
 * so the panel refreshes as fast as the SPI transfer allows. */
#define LV_DISP_DEF_REFR_PERIOD 33
#define LV_INDEV_DEF_READ_PERIOD 20

/*=======================
   FEATURE CONFIGURATION
 *=======================*/
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_DISP_ROT_MAX_BUF (10U * 1024U)

/*-----------
 * Logging
 *-----------*/
#define LV_USE_LOG 0

/*-------------
 * Asserts
 *-----------*/
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while (1);

/*-------------
 * Others
 *-----------*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_REFR_DEBUG 0
#define LV_SPRINTF_CUSTOM 0
#define LV_USE_USER_DATA 1

/*==================
 *   FONT USAGE
 *===================*/
/* Built-in Montserrat sizes SquareLine may reference as fallback */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*=================
 *  WIDGET USAGE
 *================*/
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_METER 1
#define LV_USE_SCALE 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1

/*==================
 * EXAMPLES / DEMOS
 *===================*/
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
