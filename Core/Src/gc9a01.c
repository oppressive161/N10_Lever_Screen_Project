/**
  ******************************************************************************
  * @file    gc9a01.c
  * @brief   GC9A01A 240x240 round LCD driver (SPI, write-only, no framebuffer)
  ******************************************************************************
  */
#include "gc9a01.h"

static SPI_HandleTypeDef *lcd_spi;
static uint8_t linebuf[512]; /* streaming buffer for fills/glyph rows */

#define SPI_TIMEOUT 100u

static inline void cs_low(void)  { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(GC9A01_CS_PORT, GC9A01_CS_PIN, GPIO_PIN_SET); }
static inline void dc_cmd(void)  { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_RESET); }
static inline void dc_data(void) { HAL_GPIO_WritePin(GC9A01_DC_PORT, GC9A01_DC_PIN, GPIO_PIN_SET); }

volatile HAL_StatusTypeDef gc9a01_last_status = HAL_OK;
volatile uint32_t gc9a01_spi_error = HAL_SPI_ERROR_NONE;
volatile uint32_t gc9a01_dma_count;
volatile uint32_t gc9a01_dma_error_count;
static void (*dma_complete)(void *);
static void *dma_context;

static void lcd_write(const uint8_t *data, uint16_t len)
{
  gc9a01_last_status = HAL_SPI_Transmit(lcd_spi, data, len, SPI_TIMEOUT);
  if (gc9a01_last_status != HAL_OK)
  {
    gc9a01_spi_error = HAL_SPI_GetError(lcd_spi);
    cs_high();
    HAL_GPIO_WritePin(GC9A01_BL_PORT, GC9A01_BL_PIN, GPIO_PIN_RESET);
    Error_Handler();
  }
}

static void wr_cmd(uint8_t cmd)
{
  dc_cmd();
  cs_low();
  lcd_write(&cmd, 1);
  cs_high();
}

static void wr_data(const uint8_t *data, uint16_t len)
{
  dc_data();
  cs_low();
  lcd_write(data, len);
  cs_high();
}

static void wr_cmd_data(uint8_t cmd, const uint8_t *data, uint16_t len)
{
  wr_cmd(cmd);
  if (len > 0u)
  {
    wr_data(data, len);
  }
}

/* GC9A01A init sequence (per Waveshare/Adafruit reference code).
 * Format: cmd, nargs, args... ; 0xFF nargs = end. */
static const uint8_t init_seq[] = {
  0xEF, 0,
  0xEB, 1, 0x14,
  0xFE, 0,
  0xEF, 0,
  0xEB, 1, 0x14,
  0x84, 1, 0x40,
  0x85, 1, 0xFF,
  0x86, 1, 0xFF,
  0x87, 1, 0xFF,
  0x88, 1, 0x0A,
  0x89, 1, 0x21,
  0x8A, 1, 0x00,
  0x8B, 1, 0x80,
  0x8C, 1, 0x01,
  0x8D, 1, 0x01,
  0x8E, 1, 0xFF,
  0x8F, 1, 0xFF,
  0xB6, 2, 0x00, 0x20,
  0x36, 1, 0x08,             /* MADCTL: BGR order */
  0x3A, 1, 0x05,             /* 16 bpp */
  0x90, 4, 0x08, 0x08, 0x08, 0x08,
  0xBD, 1, 0x06,
  0xBC, 1, 0x00,
  0xFF, 3, 0x60, 0x01, 0x04,
  0xC3, 1, 0x13,
  0xC4, 1, 0x13,
  0xC9, 1, 0x22,
  0xBE, 1, 0x11,
  0xE1, 2, 0x10, 0x0E,
  0xDF, 3, 0x21, 0x0C, 0x02,
  0xF0, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
  0xF1, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
  0xF2, 6, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
  0xF3, 6, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
  0xED, 2, 0x1B, 0x0B,
  0xAE, 1, 0x77,
  0xCD, 1, 0x63,
  0x70, 9, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
  0xE8, 1, 0x34,
  0x62, 12, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
            0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
  0x63, 12, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
            0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
  0x64, 7, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
  0x66, 10, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
  0x67, 10, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
  0x74, 7, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
  0x98, 2, 0x3E, 0x07,
  0x35, 0,                   /* tearing effect on */
  0x21, 0,                   /* display inversion on (panel requires it) */
  0x00, 0xFF                 /* end marker */
};

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  uint8_t b[4];

  b[0] = (uint8_t)(x0 >> 8); b[1] = (uint8_t)x0;
  b[2] = (uint8_t)(x1 >> 8); b[3] = (uint8_t)x1;
  wr_cmd_data(0x2A, b, 4); /* CASET */

  b[0] = (uint8_t)(y0 >> 8); b[1] = (uint8_t)y0;
  b[2] = (uint8_t)(y1 >> 8); b[3] = (uint8_t)y1;
  wr_cmd_data(0x2B, b, 4); /* RASET */

  wr_cmd(0x2C);            /* RAMWR */
}

/* RGB565 byte stream, with bounded chunks for the blocking SPI timeout. */
void GC9A01_Blit(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  const uint8_t *data, uint32_t byte_len)
{
  set_window(x0, y0, x1, y1);
  dc_data();
  cs_low();
  while (byte_len > 0u) {
    uint16_t n = byte_len > 19200u ? 19200u : (uint16_t)byte_len;
    lcd_write(data, n);
    data += n;
    byte_len -= n;
  }
  cs_high();
}

HAL_StatusTypeDef GC9A01_BlitDMA(uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1,
                                const uint8_t *data, uint32_t byte_len,
                                void (*complete)(void *), void *context)
{
  if (byte_len == 0u || byte_len > 65535u || dma_complete != NULL)
  {
    return HAL_BUSY;
  }

  set_window(x0, y0, x1, y1);
  dma_complete = complete;
  dma_context = context;
  dc_data();
  cs_low();
  gc9a01_last_status = HAL_SPI_Transmit_DMA(lcd_spi, data, (uint16_t)byte_len);
  if (gc9a01_last_status != HAL_OK)
  {
    cs_high();
    gc9a01_spi_error = HAL_SPI_GetError(lcd_spi);
    dma_complete = NULL;
    dma_context = NULL;
    ++gc9a01_dma_error_count;
  }
  return gc9a01_last_status;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != lcd_spi || dma_complete == NULL)
  {
    return;
  }
  void (*complete)(void *) = dma_complete;
  void *context = dma_context;
  dma_complete = NULL;
  dma_context = NULL;
  cs_high();
  ++gc9a01_dma_count;
  complete(context);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != lcd_spi || dma_complete == NULL)
  {
    return;
  }
  void (*complete)(void *) = dma_complete;
  void *context = dma_context;
  dma_complete = NULL;
  dma_context = NULL;
  cs_high();
  gc9a01_last_status = HAL_ERROR;
  gc9a01_spi_error = HAL_SPI_GetError(hspi);
  ++gc9a01_dma_error_count;
  complete(context);
}

void GC9A01_Init(SPI_HandleTypeDef *hspi)
{
  lcd_spi = hspi;
  gc9a01_last_status = HAL_OK;
  gc9a01_spi_error = HAL_SPI_ERROR_NONE;
  gc9a01_dma_count = 0u;
  gc9a01_dma_error_count = 0u;
  dma_complete = NULL;
  dma_context = NULL;
  cs_high();
  HAL_GPIO_WritePin(GC9A01_BL_PORT, GC9A01_BL_PIN, GPIO_PIN_RESET);

  /* hardware reset */
  HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GC9A01_RST_PORT, GC9A01_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(120);

  const uint8_t *p = init_seq;
  for (;;)
  {
    uint8_t cmd = *p++;
    uint8_t n = *p++;
    if (n == 0xFFu)
    {
      break;
    }
    wr_cmd_data(cmd, p, n);
    p += n;
  }

  wr_cmd(0x11); /* sleep out */
  HAL_Delay(120);
  wr_cmd(0x29); /* display on */
  HAL_Delay(20);

  GC9A01_FillScreen(COL_BLACK);
  HAL_GPIO_WritePin(GC9A01_BL_PORT, GC9A01_BL_PIN, GPIO_PIN_SET);
}

void GC9A01_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > (int16_t)GC9A01_WIDTH)  { w = (int16_t)GC9A01_WIDTH - x; }
  if (y + h > (int16_t)GC9A01_HEIGHT) { h = (int16_t)GC9A01_HEIGHT - y; }
  if (w <= 0 || h <= 0)
  {
    return;
  }

  set_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

  uint32_t total = (uint32_t)w * (uint32_t)h * 2u;
  uint16_t chunk = (uint16_t)((total < sizeof(linebuf)) ? total : sizeof(linebuf));
  for (uint16_t i = 0; i < chunk; i += 2u)
  {
    linebuf[i] = (uint8_t)(color >> 8);
    linebuf[i + 1u] = (uint8_t)color;
  }

  dc_data();
  cs_low();
  while (total > 0u)
  {
    uint16_t n = (uint16_t)((total < chunk) ? total : chunk);
    lcd_write(linebuf, n);
    total -= n;
  }
  cs_high();
}

void GC9A01_FillScreen(uint16_t color)
{
  GC9A01_FillRect(0, 0, (int16_t)GC9A01_WIDTH, (int16_t)GC9A01_HEIGHT, color);
}

/* ------------------------------------------------------------------------- */
/* Minimal 5x8 font (column bytes, LSB = top row). Subset used by the UI.    */
typedef struct
{
  char ch;
  uint8_t col[5];
} Glyph;

static const Glyph font[] = {
  { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
  { '+', { 0x08, 0x08, 0x3E, 0x08, 0x08 } },
  { '-', { 0x08, 0x08, 0x08, 0x08, 0x08 } },
  { '.', { 0x00, 0x60, 0x60, 0x00, 0x00 } },
  { ':', { 0x00, 0x36, 0x36, 0x00, 0x00 } },
  { '?', { 0x02, 0x01, 0x51, 0x09, 0x06 } },
  { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
  { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
  { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
  { '3', { 0x21, 0x41, 0x45, 0x4B, 0x31 } },
  { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
  { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
  { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
  { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
  { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
  { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
  { 'A', { 0x7E, 0x11, 0x11, 0x11, 0x7E } },
  { 'B', { 0x7F, 0x49, 0x49, 0x49, 0x36 } },
  { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
  { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
  { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
  { 'F', { 0x7F, 0x09, 0x09, 0x09, 0x01 } },
  { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
  { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
  { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
  { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
  { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
  { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
  { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
  { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
  { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
  { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
  { 'U', { 0x3F, 0x40, 0x40, 0x40, 0x3F } },
  { 'V', { 0x1F, 0x20, 0x40, 0x20, 0x1F } },
  { 'W', { 0x3F, 0x40, 0x38, 0x40, 0x3F } },
  { 'X', { 0x63, 0x14, 0x08, 0x14, 0x63 } },
};

static const uint8_t *glyph_cols(char c)
{
  for (uint8_t i = 0; i < (uint8_t)(sizeof(font) / sizeof(font[0])); i++)
  {
    if (font[i].ch == c)
    {
      return font[i].col;
    }
  }
  return font[0].col; /* unknown -> space */
}

static void draw_char(int16_t x, int16_t y, char c, uint8_t scale,
                      uint16_t fg, uint16_t bg)
{
  const uint8_t *cols = glyph_cols(c);
  uint16_t cw = (uint16_t)(6u * scale); /* 5 px glyph + 1 px spacing */

  if (x < 0 || y < 0 ||
      (x + (int16_t)cw) > (int16_t)GC9A01_WIDTH ||
      (y + (int16_t)(8u * scale)) > (int16_t)GC9A01_HEIGHT)
  {
    return;
  }
  if ((uint32_t)cw * 2u > sizeof(linebuf))
  {
    return;
  }

  set_window((uint16_t)x, (uint16_t)y,
             (uint16_t)(x + (int16_t)cw - 1),
             (uint16_t)(y + (int16_t)(8u * scale) - 1));

  dc_data();
  cs_low();
  for (uint8_t row = 0; row < 8u; row++)
  {
    /* build one scaled pixel row of the glyph */
    uint16_t i = 0;
    for (uint8_t col = 0; col < 6u; col++)
    {
      uint16_t px = (col < 5u && ((cols[col] >> row) & 1u)) ? fg : bg;
      for (uint8_t s = 0; s < scale; s++)
      {
        linebuf[i++] = (uint8_t)(px >> 8);
        linebuf[i++] = (uint8_t)px;
      }
    }
    /* repeat the row `scale` times vertically */
    for (uint8_t s = 0; s < scale; s++)
    {
      lcd_write(linebuf, i);
    }
  }
  cs_high();
}

void GC9A01_DrawText(int16_t x, int16_t y, const char *s, uint8_t scale,
                     uint16_t fg, uint16_t bg)
{
  while (*s != '\0')
  {
    draw_char(x, y, *s++, scale, fg, bg);
    x += (int16_t)(6u * scale);
  }
}
