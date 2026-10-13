# N10 final product firmware readiness

## Release blockers

1. **Angle acquisition and safety state**
   - Repair and validate CH1 hardware before treating the two TLE5014 channels as redundant.
   - Move TLE acquisition out of the LVGL task into a fixed-period sensor task.
   - Publish one timestamped snapshot to UI/CAN; UI must never own the control value.
   - Add CRC/STAT/stale checks, consecutive-good/bad hysteresis, channel-difference limits and an explicit invalid/safe state.
   - Define zero offset, direction, usable travel, end stops, filtering and calibration limits.
   - Remove the runtime diagnostic transports from the release build and use one validated SSC speed.

2. **Product communications**
   - FDCAN is configured for classic CAN at 500 kbit/s but is not started and has no application filters, RX/TX handling or timeout behavior.
   - Define message IDs, scaling, endianness, update rate, rolling counter, application CRC, timeout and bus-off recovery.
   - Keep RPM, gear, battery and other UI data invalid until a fresh valid message is received.

3. **Supervision and recovery**
   - Add IWDG and feed it only from a supervisor after required task heartbeats advance.
   - Replace infinite loops in `Error_Handler`, fault handlers, FreeRTOS assertions and LVGL assertions with fault capture followed by a controlled watchdog reset.
   - Store reset cause, fault registers, firmware version and a bounded reboot counter in retained/no-init RAM or protected nonvolatile storage.
   - Enter a visible degraded state when sensor, touch, display DMA or CAN recovery fails.

4. **Configuration and calibration storage**
   - Use a versioned record with length, CRC and validity marker.
   - Use two slots or an append-only page so power loss cannot destroy the last valid calibration.
   - Validate all stored ranges before use and retain factory defaults.

## Required hardening

- Prefer static allocation for production tasks/queues. Enable stack-overflow and malloc-failure hooks; record per-task high-water marks and LVGL heap minimum.
- Use `vTaskDelayUntil`/`osDelayUntil` for deterministic sensor and CAN periods.
- Add bounded peripheral recovery: SPI abort/re-init, I2C bus recovery, touch reset and CAN bus-off restart.
- Give every externally consumed value a validity flag and age. Never continue displaying or transmitting stale angle data as current.
- Add firmware semantic version, build identifier, hardware revision and calibration version to diagnostics.
- Decide the field-update method: protected bootloader over CAN/SWD, image integrity check and rollback/recovery behavior.
- Configure brownout/reset options and record RCC reset flags before clearing them.
- Lock unused debug access for production only after service/update requirements are defined.

## Display and UI release checks

- Current architecture is correct for GC9A01A GRAM: partial double buffers, invalid-area rendering and SPI TX DMA; no MCU full-screen framebuffer is required.
- Qualify 80 MHz SPI over minimum/maximum supply, temperature, cable/display lots and EMC. Use 60 MHz in production if the measured timing/signal-integrity margin is insufficient.
- Run page animation, rapid touch and continuous angle motion together for at least 24 hours while checking DMA/SPI errors, task heartbeat, stack margin and visual artifacts.
- Add backlight brightness control and inactivity behavior if required by the product power/thermal budget.
- The PCB exposes no LCD tearing-effect signal, so synchronization to panel scan cannot be added in firmware; validate that partial updates do not produce objectionable tearing.

## Verification and manufacturing

- Add host tests for TLE CRC, signed angle conversion, wraparound, filtering, channel plausibility and CAN encoding/decoding.
- Add hardware-in-loop fault tests: disconnected sensor, stuck data/clock, bad CRC, frozen angle, I2C timeout, CAN silence/bus-off and forced DMA error.
- Add a manufacturing-test mode for display colors, touch coordinates, both angle channels, CAN TX/RX, supply/reset cause and firmware identity.
- Define release build flags, warnings-as-errors, reproducible ELF/BIN plus checksums and a signed release record.
- Acceptance requires cold/warm starts, repeated power interruption, brownout, long-run, temperature and EMC tests on multiple production-representative boards.

## Recommended implementation order

1. Fix/verify TLE5014 CH1 hardware and specify the valid lever-angle behavior.
2. Create sensor service + validated angle snapshot + calibration model.
3. Implement CAN protocol and stale-data behavior.
4. Add supervisor, IWDG, fault capture and controlled recovery.
5. Add persistent calibration/settings and firmware identity.
6. Add bootloader/update path and manufacturing diagnostics.
7. Complete environmental, EMC, endurance and fault-injection qualification.
