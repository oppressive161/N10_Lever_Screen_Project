/**
 * GC9A01A 240x240 LCD, adapted from the STM32G474 N10 project.
 * STM32U585 board: SPI1 PA1=SCK, PA7=MOSI; PB2=CS,
 * PA8=DC, PA9=RST, PA10=backlight (active high).
 * Blocking API; owned exclusively by the display task.
 */
#ifndef GC9A01_H
#define GC9A01_H

#include "main.h"

#define GC9A01_WIDTH  240u
#define GC9A01_HEIGHT 240u
#define GC9A01_CS_PORT  LCD_CS_GPIO_Port
#define GC9A01_CS_PIN   LCD_CS_Pin
#define GC9A01_DC_PORT  LCD_DC_GPIO_Port
#define GC9A01_DC_PIN   LCD_DC_Pin
#define GC9A01_RST_PORT LCD_RST_GPIO_Port
#define GC9A01_RST_PIN  LCD_RST_Pin
#define GC9A01_BL_PORT  LCD_BL_GPIO_Port
#define GC9A01_BL_PIN   LCD_BL_Pin

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | \
                                   (((g) & 0xFCu) << 3) | (((b) & 0xF8u) >> 3)))
#define COL_BLACK RGB565(0, 0, 0)
#define COL_WHITE RGB565(255, 255, 255)
#define COL_CYAN  RGB565(56, 200, 255)
#define COL_RED   RGB565(255, 0, 0)
#define COL_GREEN RGB565(0, 255, 0)
#define COL_BLUE  RGB565(0, 0, 255)

/* Inspect in the debugger if a transfer fails. */
extern volatile HAL_StatusTypeDef gc9a01_last_status;
extern volatile uint32_t gc9a01_spi_error;
extern volatile uint32_t gc9a01_dma_count;
extern volatile uint32_t gc9a01_dma_error_count;

void GC9A01_Blit(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  const uint8_t *data, uint32_t byte_len);
HAL_StatusTypeDef GC9A01_BlitDMA(uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1,
                                const uint8_t *data, uint32_t byte_len,
                                void (*complete)(void *), void *context);
void GC9A01_Init(SPI_HandleTypeDef *hspi);
void GC9A01_FillScreen(uint16_t color);
void GC9A01_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void GC9A01_DrawText(int16_t x, int16_t y, const char *s, uint8_t scale,
                     uint16_t fg, uint16_t bg);
#endif
