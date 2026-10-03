# STM32F446 Peripheral Drivers

Bare-metal peripheral drivers for the STM32F446RE, written from the reference
manual (RM0390), with no dependency on ST HAL or CMSIS.

The API follows ST HAL conventions (`Instance` / `Config` handles,
`XXX_Init`, callbacks) but stays small: one `.h` and one `.c` per peripheral,
every register access visible and commented.

Target board: **NUCLEO-F446RE**. Toolchain: **STM32CubeIDE** (arm-none-eabi-gcc).

## Drivers

| Driver | Files | Status |
|--------|-------|--------|
| GPIO   | `stm32f446xx_gpio.*` | input, output, alternate function, EXTI interrupts |
| SPI    | `stm32f446xx_spi.*`  | master/slave, 8/16-bit, polling and interrupt |
| I2C    | `stm32f446xx_i2c.*`  | master polling and interrupt, slave via callbacks |
| UART   | `stm32f446xx_uart.*` | **skeleton**: API and step comments, not implemented yet |
| RCC    | `stm32f446xx_rcc.*`  | reads SYSCLK / PCLK1 / PCLK2 (no clock setup yet) |
| NVIC   | `stm32f446xx_nvic.*` | enable/disable and priority for IRQ 0..96 |

`stm32f446xx.h` is the device header: memory map, register structs, clock and
reset macros, bit positions, IRQ numbers. It includes every driver header, so
an application only needs `#include "stm32f446xx.h"`.

## Project layout

```
drivers/
  Inc/        device header + one header per driver
  Src/        one source file per driver
Src/          example applications, one main() each
Startup/      vector table and reset handler
tests/        on-target driver tests (not part of the normal build)
```

## Quick start

1. Open the project in STM32CubeIDE.
2. Pick **one** example in `Src/`. Every other file with a `main()` must be
   excluded from the build: right click > Resource Configurations > Exclude
   from Build. `syscalls.c` and `sysmem.c` always stay in.
3. Build and flash the `Debug` configuration.

## Examples

| File | What it shows | Hardware |
|------|---------------|----------|
| `gpio_led.c`               | blink an LED                           | LED on PA6 |
| `gpio_button.c`            | read a button by polling               | button PC5, LED PA6 |
| `gpio_button_interrupt.c`  | read a button with an EXTI interrupt   | button PC5, LED PA6 |
| `spi_tx.c`                 | SPI master, send once                  | logic analyzer on PB13/PB15 |
| `spi_tx_pico2w.c`          | SPI master, send on button press       | Raspberry Pi Pico 2 W as slave |
| `spi_cmd_handling_pico2w.c`| SPI command / answer protocol          | Raspberry Pi Pico 2 W as slave |
| `spi_message_receive_it.c` | SPI interrupt receive, printf over SWV | Raspberry Pi Pico 2 W as slave |
| `uart_tx.c`                | UART send on button press              | none (ST-LINK virtual COM port) |
| `uart_cmd_handling_it.c`   | UART interrupt command shell           | none (ST-LINK virtual COM port) |

The UART examples need the UART driver to be implemented first.

## Using a driver

Every driver follows the same steps:

```c
#include <string.h>
#include "stm32f446xx.h"

SPI_Handle_t spi;

memset(&spi, 0, sizeof(spi));          // unused fields must be 0
spi.Instance          = SPI2;          // which peripheral
spi.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
spi.Config.BusConfig  = SPI_BUS_CONFIG_FULL_DUPLEX;
spi.Config.SclkSpeed  = SPI_SCLK_SPEED_DIV32;
spi.Config.DFF        = SPI_DFF_8BITS;
spi.Config.CPOL       = SPI_CPOL_LOW;
spi.Config.CPHA       = SPI_CPHA_LOW;
spi.Config.SSM        = SPI_SSM_EN;

SPI_PeriClockControl(SPI2, ENABLE);    // 1. clock on
SPI_Init(&spi);                        // 2. configure (peripheral still off)
SPI_PeripheralControl(SPI2, ENABLE);   // 3. switch on
SPI_SendData(SPI2, data, len);         // 4. use
```

The pins are configured separately with the GPIO driver (alternate function
mode and AF number from the datasheet).

**Interrupts:** enable the line with `XXX_IRQInterruptConfig(IRQ_NO_..., ENABLE)`,
call `XXX_IRQHandling(&handle)` from the vector (for example `SPI2_IRQHandler`),
and override the weak `XXX_ApplicationEventCallback` to get completion and error
events.

## Conventions

- **Names:** public functions, types and macros start with the module name
  (`SPI_Init`, `SPI_Handle_t`, `SPI_DFF_8BITS`). Struct members do not
  (`Config.DFF`).
- **Handles:** `Instance` is the register base, `Config` holds the settings.
- **Register bits:** use the bit position macros (`SPI_CR1_SPE`), never a raw
  number.
- **Comments:** every public function has a doc block. Comments explain *why*
  (with the RM0390 section when useful), not what the line does.
- **Callbacks:** declared `__attribute__((weak))` in the driver; the
  application overrides them without changing the driver.

## Adding a driver

1. Add the register struct, base pointer, clock/reset macros, bit positions
   and IRQ numbers to `stm32f446xx.h`.
2. Create `stm32f446xx_xxx.h` with `XXX_Config_t`, `XXX_Handle_t`, option
   macros and the API, following an existing driver.
3. Create `stm32f446xx_xxx.c`. Use `NVIC_IRQInterruptConfig` /
   `NVIC_IRQPriorityConfig` for the IRQ functions.
4. Include the new header at the end of `stm32f446xx.h`.
5. Add an example in `Src/` and a test suite in `tests/target/`.

## Tests

`tests/target/` holds one firmware image that tests every driver on the
board: real registers, real pins, real interrupts. Register tests need no
wiring; loopback tests need a few jumper wires and are skipped without them.
See [tests/README.md](tests/README.md).

## Known limitations

- Blocking functions wait without a timeout. If the hardware does not answer
  (missing slave, wrong wiring) they never return.
- RCC can read the clock tree but not configure it; the board runs on the
  16 MHz HSI.
- No DMA support.
