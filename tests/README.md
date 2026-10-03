# Tests

Tests live outside `Src/`, so the normal CubeIDE build never compiles them.
`Src/` holds only demo applications (one `main` each).

```
tests/
  target/   on-target (hardware-in-the-loop) tests, flashed to the NUCLEO-F446RE
  unit/     (planned) host-side unit tests, run on the PC with Unity/Ceedling
```

## target/

One firmware image tests every driver on real silicon: real registers, real
pins, real interrupts. Nothing is mocked.

| File              | What                                                        |
|-------------------|-------------------------------------------------------------|
| `test_main.c`     | `main`: runs all suites, drives the result LED              |
| `test_harness.*`  | `CHECK`, `SKIP`, timeouts (DWT), wire check, NVIC read back |
| `test_rcc.c`      | clock math, bad config rejected, 180 MHz and back, SysTick  |
| `test_nvic.c`     | enable/disable at register edges (0..96), priority slots    |
| `test_gpio.c`     | pin fields, AF, pull-up/down, EXTI config, EXTI interrupts  |
| `test_spi.c`      | SPI2 master: config, NVIC, polling / IT / OVR loopback      |
| `test_i2c.c`      | I2C1 master <-> I2C3 slave: timing regs, ACK, IT + blocking |
| `test_uart.c`     | USART1: config, BRR, flags, polling / IT loopback           |

### Wiring

Without any wire, all register-level tests run and every loopback test
reports `SKIP` (not `FAIL`). Add wires to enable the loopback tests.

| Suite | Connect                                   | Nucleo header             |
|-------|-------------------------------------------|---------------------------|
| GPIO  | PC10 <-> PC11                             | CN7 pin 1 - pin 2 (jumper cap) |
| SPI   | PB15 MOSI <-> PB14 MISO                   | CN10 pin 26 - pin 28 (jumper cap) |
| I2C   | PB8 <-> PA8 (SCL), PB9 <-> PC9 (SDA), 4.7k pull-up to 3V3 on each line | D15 - D7, D14 - CN10 pin 1 |
| UART  | PA9 TX <-> PA10 RX                        | D8 - D2                   |

Must stay free: PC8 (pull-up/down test reads it), PA5 (LD2 result LED),
PA13/PA14 (SWD), PB3 (SWO).

### Build and run in CubeIDE

Use a separate build configuration, so `Debug` (the apps) stays untouched:

1. Project > Build Configurations > Manage > New: `Test`, copy from `Debug`.
2. With `Test` active, Project > Properties > C/C++ General > Paths and
   Symbols:
   - Source Location: add `tests/target`
   - Includes (GNU C): add `tests/target`
3. In `Test`, exclude every file in `Src/` that has a `main` (right click >
   Resource Configurations > Exclude from Build). Keep `syscalls.c` and
   `sysmem.c`.
4. Build `Test`, start a debug session, open the SWV ITM Data Console
   (enable port 0, core clock 16 MHz), resume.

### Reading the result

```
=== suite: spi ===
[RUN ] spi_nvic_config
    FAIL test_spi.c:190: SPI2 (IRQ 36) priority not 11
[FAIL] spi_nvic_config (1 checks)
...
===== 53 tests: 30 passed, 17 failed, 6 skipped =====   (example numbers)
```

- LD2 steady ON: no failures (skips allowed). Blinking: something failed.
- `g_test_summary` in Live Expressions: counters, first failed file/line, and
  `current_test`. If the board hangs, `current_test` names the test.

### Adding a test

1. Write `static void test_xxx(void)` in the suite file. Start from a clean
   peripheral (DeInit + fresh handle).
2. Read registers directly in `CHECK`, not through the driver under test.
3. Wait only with `test_wait_count` / `test_wait_reg` (they time out).
4. Loopback test: call the suite's wire check first and `SKIP` without it.
5. Register it in the suite's `test_suite_xxx()` list.

## unit/ (planned)

Host tests run in milliseconds and in CI, without a board. The driver takes a
register block pointer (`pUARTx`, `pSPIx`), so a test can pass a register
struct in RAM and check what the driver wrote. Code that uses fixed addresses
(`RCC`, `NVIC_*`, `pSPIx == SPI1` checks) needs those macros redirected to
fake structs in a test-only header.
