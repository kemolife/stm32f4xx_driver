/*
 * test_harness.h
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  Minimal on-target test harness shared by all driver suites.
 *
 *  Rules for suites:
 *    - Start every test from a clean peripheral (DeInit + fresh handle).
 *    - Check hardware registers directly, not through the driver under test.
 *    - Every wait has a timeout (test_wait_*). A missing feature must FAIL,
 *      not hang. Driver calls that have no timeout of their own are only made
 *      after a precondition proved they will return.
 *    - Loopback tests call test_wire_connected() first and SKIP without the wire.
 *    - Never touch PA13/PA14 (SWD), PB3 (SWO) or PA5 (result LED).
 */
#ifndef TEST_HARNESS_H_
#define TEST_HARNESS_H_

#include <stdint.h>
#include <stdio.h>
#include "stm32f446xx.h"

/* ========================================================================== */
/*   Result summary, watch it in the debugger (Live Expressions)              */
/* ========================================================================== */

typedef struct {
	uint32_t tests_run;
	uint32_t tests_passed;
	uint32_t tests_failed;
	uint32_t tests_skipped;
	uint32_t checks_failed;
	const char *first_fail_file;  /* file and line of the first failed CHECK  */
	uint32_t first_fail_line;
	const char *current_test;     /* name of the test running now. If the     */
	                              /* board hangs, this is where               */
	uint8_t  done;                /* 1 when all suites have finished          */
} TestSummary_t;

extern volatile TestSummary_t g_test_summary;

/* ========================================================================== */
/*   Assertions and test runner                                               */
/* ========================================================================== */

#define CHECK(cond, msg)  test_check(((cond) ? 1 : 0), (msg), __FILE__, __LINE__)
#define SKIP(reason)      test_skip(reason)

void test_harness_init(void);
void test_suite_begin(const char *name);
void test_run(void (*fn)(void), const char *name);
void test_check(int ok, const char *msg, const char *file, int line);
void test_skip(const char *reason);
void test_print_summary(void);

/* ========================================================================== */
/*   Time and waits (DWT cycle counter, independent of any driver)            */
/* ========================================================================== */

#define TEST_TIMEOUT_MS   200U   /* default: far longer than any frame used here */

void test_delay_ms(uint32_t ms);

/* Raw CPU cycle counter (DWT CYCCNT). Counts HCLK cycles, wraps after 2^32.
 * Note: test_delay_ms / test_wait_* assume 16 MHz; do not use them while a
 * test has switched to another clock. */
uint32_t test_cycles(void);

/* Waits until *counter >= target. Returns 1 on success, 0 on timeout. */
int test_wait_count(volatile uint8_t *counter, uint8_t target, uint32_t timeout_ms);

/* Waits until all bits in mask are set (set = 1) or cleared (set = 0) in *reg. */
int test_wait_reg(volatile uint32_t *reg, uint32_t mask, int set, uint32_t timeout_ms);

/* ========================================================================== */
/*   Hardware helpers (raw registers, do not depend on the GPIO driver)       */
/* ========================================================================== */

/* Returns 1 if a wire connects outPort/outPin to inPort/inPin.
 * Drives the out pin high and low and reads the in pin, with the in pin pulled
 * the opposite way each time. Leaves both pins as inputs without pull. */
int test_wire_connected(GPIO_RegDef_t *outPort, uint8_t outPin,
                        GPIO_RegDef_t *inPort, uint8_t inPin);

/* Reads back the NVIC state for an IRQ number. */
int     test_nvic_is_enabled(uint8_t irq);
uint8_t test_nvic_priority(uint8_t irq);   /* returns the 4-bit priority value */
void    test_nvic_disable_all_used(void);  /* cleanup between suites */

void test_led_init(void);
void test_led_write(int on);
void test_led_toggle(void);

/* ========================================================================== */
/*   Suites                                                                   */
/* ========================================================================== */

void test_suite_rcc(void);
void test_suite_nvic(void);
void test_suite_gpio(void);
void test_suite_spi(void);
void test_suite_i2c(void);
void test_suite_uart(void);

#endif /* TEST_HARNESS_H_ */
