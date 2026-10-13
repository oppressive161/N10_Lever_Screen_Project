/* HAL timeouts use the FreeRTOS 1 ms tick once the scheduler is running.
 * TIM6 is retained as the HAL time base during early boot, before RTOS starts.
 * This also prevents SPI/I2C timeout loops from stalling if TIM6 IRQ is
 * suspended after the UI task begins.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32u5xx_hal.h"
extern __IO uint32_t uwTick;
uint32_t HAL_GetTick(void)
{
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
  {
    return (uint32_t)xTaskGetTickCount();
  }
  return uwTick;
}
