/* The LCD controller GRAM is the framebuffer. LVGL renders only invalidated
 * regions into two partial buffers while SPI1 GPDMA transmits the other one. */
#include "lv_port.h"
#include "lvgl/lvgl.h"
#include "gc9a01.h"
#include "cmsis_os2.h"
#define DRAW_LINES 60
static lv_color_t pixels_a[GC9A01_WIDTH * DRAW_LINES] __attribute__((aligned(4)));
static lv_color_t pixels_b[GC9A01_WIDTH * DRAW_LINES] __attribute__((aligned(4)));
static lv_disp_draw_buf_t buffer;
static lv_disp_drv_t driver;
volatile uint32_t ui_flush_count;
volatile uint32_t ui_flush_dma_errors;
static lv_disp_drv_t * volatile completed_driver;

static void flush_complete(void *context)
{
  /* SPI IRQ context: leave all LVGL state changes to the screen task. */
  completed_driver = (lv_disp_drv_t *)context;
}

void lv_port_process(void)
{
  lv_disp_drv_t *done;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  done = completed_driver;
  completed_driver = NULL;
  if (!primask) __enable_irq();

  if (done != NULL) {
    ++ui_flush_count;
    lv_disp_flush_ready(done);
  }
}

static void wait_flush(lv_disp_drv_t *drv)
{
  lv_port_process();
  if (drv->draw_buf->flushing) osThreadYield();
}

static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *colors)
{
  uint32_t count = (uint32_t)(a->x2-a->x1+1) * (uint32_t)(a->y2-a->y1+1);
  uint32_t *p32 = (uint32_t *)colors;
  uint32_t pairs = count / 2u;
  for (uint32_t i=0; i<pairs; ++i) p32[i] = __REV16(p32[i]);
  if (count & 1u) {
    uint16_t *last = (uint16_t *)&p32[pairs];
    *last = __REV16(*last);
  }
  if (GC9A01_BlitDMA(a->x1, a->y1, a->x2, a->y2,
                     (const uint8_t *)colors, count*2u,
                     flush_complete, drv) != HAL_OK)
  {
    ++ui_flush_dma_errors;
    lv_disp_flush_ready(drv);
  }
}
void lv_port_init(void)
{
  completed_driver = NULL;
  lv_disp_draw_buf_init(&buffer, pixels_a, pixels_b, GC9A01_WIDTH * DRAW_LINES);
  lv_disp_drv_init(&driver);
  driver.hor_res=GC9A01_WIDTH;
  driver.ver_res=GC9A01_HEIGHT;
  driver.flush_cb=flush;
  driver.wait_cb=wait_flush;
  driver.draw_buf=&buffer;
  driver.full_refresh=0;
  driver.direct_mode=0;
  if (lv_disp_drv_register(&driver)==NULL) Error_Handler();
}
