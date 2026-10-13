/* Original SquareLine screens on STM32U585. All LVGL calls share this task. */
#include "main.h"
#include "app_freertos.h"
#include "screen_app.h"
#include "gc9a01.h"
#include "cst816s.h"
#include "tle5014.h"
#include "lv_port.h"
#include "ui.h"
#include <stdbool.h>
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;
extern uint32_t _sdata, _edata, _sidata, _end;
volatile uint32_t ui_stage, ui_heartbeat, ui_page, ui_touch_ok;
volatile uint32_t ui_touch_events, ui_touch_errors, ui_stack_free_words;
volatile uint32_t ui_touch_x, ui_touch_y;

static lv_obj_t *angle_labels[2];
volatile uint32_t ui_angle_source; /* 0=no valid angle, 1=CH1, 2=CH2; display only. */
static void update_angles(void)
{
  TLE5014_Poll();
  for(unsigned i=0;i<2;i++) {
    const volatile TLE5014_Reading *r=&tle_readings[i];
    if(r->status==TLE5014_OK) {
      int32_t a=r->centideg, magnitude=a<0?-a:a;
      lv_label_set_text_fmt(angle_labels[i],"CH%u %s%ld.%02ld deg",i+1,
                           a<0?"-":"+",(long)(magnitude/100),(long)(magnitude%100));
    } else {
      const char *e=r->status==TLE5014_ERR_SPI?"SPI":r->status==TLE5014_ERR_CRC?"CRC":"STAT";
      lv_label_set_text_fmt(angle_labels[i],"CH%u %s %04X",i+1,e,(unsigned)r->safety);
    }
  }
  /* Keep each channel's error visible. This selection is for the gauge only,
   * not a redundant control/safety decision or a calibrated channel fusion. */
  ui_angle_source=tle_readings[0].status==TLE5014_OK?1u:
                  tle_readings[1].status==TLE5014_OK?2u:0u;
  if(ui_angle_source) {
    int32_t a=tle_readings[ui_angle_source-1u].centideg;
    int16_t deg=(int16_t)((a/100+360)%360);
    lv_obj_clear_flag(ui_anglearrowimage,LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ui_anglearrowimage,LV_ALIGN_CENTER,
                 86*lv_trigo_sin(deg)/32767,-86*lv_trigo_cos(deg)/32767);
    lv_img_set_angle(ui_anglearrowimage,(int16_t)(((a/10)+1800+3600)%3600));
  } else lv_obj_add_flag(ui_anglearrowimage,LV_OBJ_FLAG_HIDDEN);
}

static void update_stats(void)
{
  /* Static allocation includes the reserved RTOS and LVGL heaps. */
  uint32_t ram = (uint32_t)&_end - 0x20000000u;
  uint32_t flash = (uint32_t)&_sidata - 0x08000000u +
                   (uint32_t)&_edata - (uint32_t)&_sdata;
  lv_label_set_text_fmt(ui_ramusage, "RAM %lu%%", (unsigned long)(ram*100u/(768u*1024u)));
  lv_label_set_text_fmt(ui_flashsize, "FLASH %lu%%", (unsigned long)(flash*100u/(2048u*1024u)));
  lv_label_set_text(ui_cpuusage, "CPU 160 MHz");
}
void ScreenApp_Run(void)
{
  ui_stage=1;
  GC9A01_Init(&hspi1);
  ui_touch_ok=CST816S_Init(&hi2c1);
  TLE5014_Init();
  ui_stage=2;
  lv_init();
  lv_port_init();
  ui_init();
  /* No live sensor data yet: do not display generated example measurements. */
  lv_label_set_text(ui_gearlabel, "-");
  lv_label_set_text(ui_rpmlabel, "----");
  lv_obj_add_flag(ui_geararrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_anglearrowimage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_opa(ui_Screen2, LV_OPA_COVER, LV_PART_MAIN|LV_STATE_DEFAULT);
  for(unsigned i=0;i<2;i++) {
    angle_labels[i]=lv_label_create(ui_Screen2);
    lv_obj_set_style_text_font(angle_labels[i],&lv_font_montserrat_12,0);
    lv_obj_align(angle_labels[i],LV_ALIGN_CENTER,0,42+(int)i*17);
  }
  update_angles();
  update_stats();
  lv_refr_now(NULL);
  ui_stage=3;
  uint32_t start=osKernelGetTickCount(), last_switch=start, last_stats=start, last_angles=start;
  bool was_down=false;
  lv_obj_t *pages[4]={ui_Screen2,ui_Screen3,ui_Screen4,ui_Screen5};
  for (;;) {
    uint32_t now=osKernelGetTickCount();
    if (!ui_page && now-start>=2000u) {
      ui_page=1;
      lv_scr_load_anim(pages[0], LV_SCR_LOAD_ANIM_FADE_ON, 50, 0, false);
    }
    if (ui_touch_ok) {
      CST816S_Touch t;
      if (CST816S_Read(&hi2c1,&t)) {
        bool down=t.fingers>0u;
        if (down && !was_down) {
          ++ui_touch_events;
          ui_touch_x=t.x; ui_touch_y=t.y;
          if (ui_page && now-last_switch>=400u) {
            uint32_t next=ui_page;
            if (t.x>=120u && next<4u) ++next;
            else if (t.x<120u && next>1u) --next;
            if (next!=ui_page) {
              ui_page=next;
              last_switch=now;
              lv_scr_load_anim(pages[next-1u],LV_SCR_LOAD_ANIM_FADE_ON,250,0,false);
            }
          }
        }
        was_down=down;
      } else { ++ui_touch_errors; }
    }
    if (now-last_stats>=1000u) { update_stats(); last_stats=now; }
    if(now-last_angles>=50u) { update_angles(); last_angles=now; }
    /* Complete any SPI DMA flush from task context before LVGL runs again. */
    lv_port_process();
    lv_timer_handler();
    ++ui_heartbeat;
    ui_stack_free_words=uxTaskGetStackHighWaterMark(NULL);
    osDelay(10);
  }
}
