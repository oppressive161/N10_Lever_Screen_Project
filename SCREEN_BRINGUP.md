# N10 screen and angle bring-up — 2026-09-26

## Active configuration

- STM32U585CIU6, TrustZone disabled, FreeRTOS FPU context enabled.
- HSE 16 MHz, PLL M=1 N=20 P=4 Q=4 R=2: CPU/AHB/APB 160 MHz, FDCAN kernel 80 MHz.
- Active 160 MHz uses VOS range 1 and Flash latency 4. The successful startup switches to PLL with AHB /2 (80 MHz), waits for the core supply to settle, and then changes AHB to /1 (160 MHz). Earlier direct 160 MHz transitions produced variable precise bus faults or SPI timeouts. Runtime `SystemCoreClock` was read back as 160000000 and UI/RTOS counters continued with no SPI or Cortex fault flags during the observed test.
- LCD SPI1 /2 = 80 MHz TX-only is active. This is the STM32U585 master-mode maximum at 3.3 V/VOS1 and remains within the GC9A01A 10 ns minimum serial write-cycle specification (100 MHz theoretical controller limit). The LCD clock and data GPIOs use the very-high-speed setting; the schematic has 22-ohm series resistors on both signals.
- Sensor SPI2 and SPI3 /64 = 2.5 MHz, 16-bit mode 1, half-duplex. CRC-valid frames are present in the current 160 MHz test, but both safety words report the angle-invalid status; physical angle response remains to be reconfirmed.
- I2C1 timing 0x00F07BFF is retained; with the lower peripheral clock than its original 160 MHz calculation it operates slower. CST816S reads and navigation work in this configuration.

## Imported UI

Original LVGL 8.3.11 and all 37 unmodified SquareLine export files (five screens, images and fonts) are included. Images fit in internal flash; old G474 external flash/PSRAM dependencies are not required. GC9A01A's internal 240x240 GRAM is the persistent framebuffer. The MCU therefore keeps no full-screen framebuffer: LVGL redraws only invalidated areas into two 240x60 RGB565 partial buffers. SPI1 TX sends one buffer asynchronously on GPDMA1 channel 0 while LVGL renders into the other. Full-refresh and direct modes are disabled. RGB565 wire-order conversion handles two pixels per ARM `REV16` operation before DMA starts. The IRQ callback only records completion; `lv_disp_flush_ready()` is serviced by the screen task so LVGL state is never modified from interrupt context. The project also uses a 64 KB LVGL heap, 32 KB FreeRTOS heap and an 8 KB UI task stack.

The splash appears for two seconds. Right-half taps advance gauge -> statistics -> boat -> battery; left-half taps return. CPU stats show frequency, not a fabricated CPU load. RAM percentage represents reserved static allocation, including heaps. RPM and gear remain unset because their acquisition/calibration has not been integrated.

The gauge now shows real CH1/CH2 angle readings or channel-specific error status. The arrow displays CH1 when valid, otherwise CH2 when valid; `ui_angle_source` explicitly records which source is shown. The user confirmed arrow rotation at 60 MHz LCD SPI. This is display selection only, not sensor fusion or a safety/control fallback. Both channel statuses remain visible.

## Sensor acquisition and findings

`tle5014.c` adapts the old SSC command, CRC-8/J1850 and signed 15-bit angle conversion to the new SPI peripherals. It performs read transactions only. On a bad reading it reads STAT, retaining the result and acknowledging latched sensor flags as in the old application. No sensor calibration or EEPROM was written.

- CH2: CRC-valid and angle-valid readings continue during display operation. Initially invalid-angle status cleared after reading STAT. The user confirmed a magnet is fitted and aligned and that the on-screen arrow rotates with movement.
- CH1: no valid frame at 3.75 MHz, 468.75 kHz, or GPIO SSC (approximately 250 kHz maximum). When the data pin was released as an input, PB15 read low despite the schematic's external pull-up. Enabling the internal pull-up made it read high and every returned bit was 1. This suggests a floating/open input or missing external pull-up/sensor connection; it does not establish the exact hardware fault.
- Inspect the CH1 DATA path (PB15, R11, sensor pin 1, R28), clock (PB13/R10/pin 2), CS (PB12/R9/pin 3), and channel-1 supply/GND (pins 4/5). Confirm fitted part number and orientation. The exact fitted single/dual-die variant is awaiting user confirmation.
- `tle_transport[0..1]` is optional debugger diagnosis: 0=normal hardware /32; 1=hardware /256; 2=GPIO SSC. Default/reset mode is 0 for both. Changes apply between completed transactions and never write EEPROM.

Debugger symbols: `tle_readings`, `tle_stat`, `tle_stat_safety`, `tle_stat_result`, `ui_angle_source`, `ui_heartbeat`, `ui_touch_errors`, `ui_stack_free_words`, `ui_flush_count`, `ui_flush_dma_errors`, `gc9a01_spi_error`, `gc9a01_dma_count`, `gc9a01_dma_error_count`.

## Validation and recovery

Build and ST-LINK flash readback verification passed. At 160 MHz CPU and 80 MHz LCD SPI, the DMA UI task runs without recorded SPI or DMA errors in the observed test. Non-halting debugger samples showed DMA completions increasing from 1011 to 2430; the final LVGL flush count matched at 2430 and the task heartbeat reached 5092. Touch input remained active and navigated to page 3 during the test. The user confirmed correct pixels, colors, motion and touch behavior at 80 MHz. A normal debugger memory read halts this MCU, so live counters must be sampled with direct/fast access. An earlier apparent SPI-speed freeze persisted even after restoring 15 MHz: TIM6 HAL tick interrupts had stopped at 13.1 seconds, so LVGL and UI timeout logic stopped advancing. LVGL, the UI task and sensor timestamps now use the 1 kHz FreeRTOS tick; `rtos_timebase.c` also uses it for HAL timeouts after the scheduler starts, retaining `uwTick` for early boot. The TIM6 interrupt-disable mechanism itself remains unidentified. These short bench checks do not replace long-duration hardware validation. VCAP and VDD transient behavior have not been measured on the assembled board. The verified maximum-speed DMA firmware is retained as `work/n10-lvgl-160mhz-lcd80mhz-dma.elf`; the previous 40 MHz DMA firmware is `work/n10-lvgl-160mhz-lcd40mhz-dma.elf`; the 120 MHz / 60 MHz recovery firmware is `work/n10-lvgl-120mhz-lcd60mhz-rtostick.elf`. Original full 2 MB flash was backed up before the first screen test.

References: Infineon TLE5014SP16 E0001 datasheet (SSC protocol/safety/CRC), Waveshare GC9A01A datasheet, ST STM32U585 datasheet and ES0499 errata. Board wiring was checked against Angle_Board.pdf.
