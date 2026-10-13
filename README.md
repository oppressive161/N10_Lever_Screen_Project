# N10 Magnetic Steering Lever Screen

STM32U585CIU6 firmware for the N10 magnetic steering lever display board.

## Current configuration

- STM32U585 at 160 MHz from a 16 MHz HSE
- GC9A01A 240x240 display at 80 MHz SPI1 TX
- GPDMA-backed partial double buffering with LVGL 8.3
- CST816S capacitive touch controller over I2C
- Dual TLE5014 angle-sensor interfaces over SPI2/SPI3
- FreeRTOS/CMSIS-RTOS2
- Classic CAN peripheral configured for 500 kbit/s; application protocol is pending

The display uses its internal GRAM as the persistent framebuffer. LVGL renders only invalidated regions into two 240x60 RGB565 buffers while DMA transmits the other buffer.

## Build

The project is generated for STM32Cube/CMake and requires the STM32U5 HAL/CMSIS sources included in this repository plus an Arm GNU toolchain.

```sh
cmake --preset Debug
cmake --build --preset Debug
```

## Hardware and validation notes

- [`SCREEN_BRINGUP.md`](SCREEN_BRINGUP.md) records clock, display, touch and sensor bring-up findings.
- [`PRODUCTION_READINESS.md`](PRODUCTION_READINESS.md) lists the remaining work before a production release.

The current firmware is a validated hardware bring-up build. TLE5014 channel 1 diagnosis, the final CAN application protocol, watchdog/fault recovery, calibration storage and production qualification remain open.
