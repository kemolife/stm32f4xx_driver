/*
 * test_harness.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 */

#include <string.h>
#include "test_harness.h"

/* ========================================================================== */
/*   ITM printf (SWV console, stimulus port 0)                                */
/* ========================================================================== */

#define ITM_STIM0       (*(volatile uint32_t*)0xE0000000)
#define ITM_TER         (*(volatile uint32_t*)0xE0000E00)
#define DEMCR           (*(volatile uint32_t*)0xE000EDFC)
#define DEMCR_TRCENA    (1UL << 24)

#define DWT_CTRL        (*(volatile uint32_t*)0xE0001000)
#define DWT_CYCCNT      (*(volatile uint32_t*)0xE0001004)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

static void ITM_SendChar(char ch) {
	if (!(DEMCR & DEMCR_TRCENA)) { return; }   // trace not enabled
	if (!(ITM_TER & 1UL))        { return; }   // stimulus port 0 disabled by debugger

	while (ITM_STIM0 == 0);                    // wait for port to accept a byte
	*(volatile uint8_t*)&ITM_STIM0 = (uint8_t)ch;
}

int _write(int file, char *ptr, int len) {
	(void)file;
	for (int i = 0; i < len; i++) {
		ITM_SendChar(ptr[i]);
	}
	return len;
}

/* ========================================================================== */
/*   Runner                                                                   */
/* ========================================================================== */

volatile TestSummary_t g_test_summary;

static uint32_t current_failures;
static uint8_t  current_skipped;
static uint32_t cycles_per_ms = 16000U;   /* HSI default, updated in init */

void test_harness_init(void) {
	memset((void *)&g_test_summary, 0, sizeof(g_test_summary));

	/* DWT cycle counter for timeouts. Works with or without a debugger. */
	DEMCR |= DEMCR_TRCENA;
	DWT_CYCCNT = 0;
	DWT_CTRL |= DWT_CTRL_CYCCNTENA;

	/* Timeouts are computed from the core clock. Use the raw SWS field, not the
	 * RCC driver, so a broken RCC driver cannot break every timeout. */
	uint32_t sws = (RCC->CFGR >> 2) & 0x3U;
	if (sws == 1U) {
		cycles_per_ms = HSE_VALUE / 1000U;
	} else {
		cycles_per_ms = HSI_VALUE / 1000U;   /* PLL is never started by these tests */
	}
}

void test_suite_begin(const char *name) {
	printf("\n=== suite: %s ===\n", name);
}

void test_run(void (*fn)(void), const char *name) {
	current_failures = 0;
	current_skipped = 0;
	g_test_summary.current_test = name;
	g_test_summary.tests_run++;

	printf("[RUN ] %s\n", name);
	fn();

	if (current_failures != 0U) {
		g_test_summary.tests_failed++;
		printf("[FAIL] %s (%lu checks)\n", name, (unsigned long)current_failures);
	} else if (current_skipped) {
		g_test_summary.tests_skipped++;
		printf("[SKIP] %s\n", name);
	} else {
		g_test_summary.tests_passed++;
		printf("[PASS] %s\n", name);
	}
}

void test_check(int ok, const char *msg, const char *file, int line) {
	if (ok) {
		return;
	}

	current_failures++;
	g_test_summary.checks_failed++;
	if (g_test_summary.first_fail_line == 0U) {
		g_test_summary.first_fail_file = file;
		g_test_summary.first_fail_line = (uint32_t)line;
	}

	/* Print only the file name, not the full build path */
	const char *base = strrchr(file, '/');
	printf("    FAIL %s:%d: %s\n", base ? base + 1 : file, line, msg);
}

void test_skip(const char *reason) {
	current_skipped = 1;
	printf("    SKIP: %s\n", reason);
}

void test_print_summary(void) {
	printf("\n===== %lu tests: %lu passed, %lu failed, %lu skipped =====\n",
	       (unsigned long)g_test_summary.tests_run,
	       (unsigned long)g_test_summary.tests_passed,
	       (unsigned long)g_test_summary.tests_failed,
	       (unsigned long)g_test_summary.tests_skipped);

	if (g_test_summary.tests_failed != 0U) {
		const char *base = strrchr(g_test_summary.first_fail_file, '/');
		printf("first failure: %s:%lu\n",
		       base ? base + 1 : g_test_summary.first_fail_file,
		       (unsigned long)g_test_summary.first_fail_line);
	}
}

/* ========================================================================== */
/*   Time                                                                     */
/* ========================================================================== */

void test_delay_ms(uint32_t ms) {
	uint32_t start = DWT_CYCCNT;
	uint32_t span = ms * cycles_per_ms;
	while ((DWT_CYCCNT - start) < span);
}

int test_wait_count(volatile uint8_t *counter, uint8_t target, uint32_t timeout_ms) {
	uint32_t start = DWT_CYCCNT;
	uint32_t span = timeout_ms * cycles_per_ms;

	while ((DWT_CYCCNT - start) < span) {
		if (*counter >= target) {
			return 1;
		}
	}
	return (*counter >= target) ? 1 : 0;
}

int test_wait_reg(volatile uint32_t *reg, uint32_t mask, int set, uint32_t timeout_ms) {
	uint32_t start = DWT_CYCCNT;
	uint32_t span = timeout_ms * cycles_per_ms;

	while ((DWT_CYCCNT - start) < span) {
		uint32_t v = *reg & mask;
		if (set ? (v == mask) : (v == 0U)) {
			return 1;
		}
	}
	return 0;
}

/* ========================================================================== */
/*   Raw GPIO helpers                                                         */
/* ========================================================================== */

static void raw_port_clock(GPIO_RegDef_t *port) {
	uint32_t index = ((uint32_t)port - GPIOA_BASE) / 0x400U;
	RCC->AHB1ENR |= (1U << index);
	(void)RCC->AHB1ENR;   // read back: the clock needs 2 cycles before the port is usable
}

static void raw_pin_mode(GPIO_RegDef_t *port, uint8_t pin, uint32_t mode) {
	port->MODER = (port->MODER & ~(0x3U << (2U * pin))) | (mode << (2U * pin));
}

static void raw_pin_pull(GPIO_RegDef_t *port, uint8_t pin, uint32_t pull) {
	port->PUPDR = (port->PUPDR & ~(0x3U << (2U * pin))) | (pull << (2U * pin));
}

static void raw_pin_write(GPIO_RegDef_t *port, uint8_t pin, int high) {
	port->BSRR = high ? (1U << pin) : (1U << (pin + 16U));
}

static int raw_pin_read(GPIO_RegDef_t *port, uint8_t pin) {
	return (port->IDR >> pin) & 1U;
}

int test_wire_connected(GPIO_RegDef_t *outPort, uint8_t outPin,
                        GPIO_RegDef_t *inPort, uint8_t inPin) {
	int ok = 1;

	raw_port_clock(outPort);
	raw_port_clock(inPort);

	outPort->OTYPER &= ~(1U << outPin);    // push-pull, so it wins over any pull
	raw_pin_mode(inPort, inPin, 0U);       // input
	raw_pin_mode(outPort, outPin, 1U);     // output

	/* Drive high while the input is pulled down: only a wire makes it read 1 */
	raw_pin_pull(inPort, inPin, 2U);
	raw_pin_write(outPort, outPin, 1);
	test_delay_ms(1);
	ok &= (raw_pin_read(inPort, inPin) == 1);

	/* Drive low while the input is pulled up: only a wire makes it read 0 */
	raw_pin_pull(inPort, inPin, 1U);
	raw_pin_write(outPort, outPin, 0);
	test_delay_ms(1);
	ok &= (raw_pin_read(inPort, inPin) == 0);

	/* Leave both as plain inputs, the suite configures them next */
	raw_pin_mode(outPort, outPin, 0U);
	raw_pin_pull(inPort, inPin, 0U);

	return ok;
}

/* ========================================================================== */
/*   NVIC read back                                                           */
/* ========================================================================== */

#define NVIC_ISER_BASE  ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x000UL))
#define NVIC_ICER_BASE  ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x080UL))
#define NVIC_ICPR_BASE  ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x180UL))

int test_nvic_is_enabled(uint8_t irq) {
	/* Reading ISER returns the enable state of each line */
	return (NVIC_ISER_BASE[irq / 32U] >> (irq % 32U)) & 1U;
}

uint8_t test_nvic_priority(uint8_t irq) {
	uint32_t reg = NVIC_PR_BASE_ADDR[irq / 4U];
	uint8_t byte = (uint8_t)(reg >> ((irq % 4U) * 8U));
	return (uint8_t)(byte >> (8U - NO_PR_BITS_IMPLEMENTED));
}

void test_nvic_disable_all_used(void) {
	/* Disable and un-pend every external IRQ so one suite cannot leak an
	 * interrupt into the next one */
	for (uint32_t i = 0; i < 4U; i++) {   /* 4 registers cover IRQ 0 .. 96 */
		NVIC_ICER_BASE[i] = 0xFFFFFFFFU;
		NVIC_ICPR_BASE[i] = 0xFFFFFFFFU;
	}
}

/* ========================================================================== */
/*   Result LED, LD2 on PA5                                                   */
/* ========================================================================== */

#define LED_PORT  GPIOA
#define LED_PIN   5U

void test_led_init(void) {
	raw_port_clock(LED_PORT);
	LED_PORT->OTYPER &= ~(1U << LED_PIN);
	raw_pin_pull(LED_PORT, LED_PIN, 0U);
	raw_pin_mode(LED_PORT, LED_PIN, 1U);
	raw_pin_write(LED_PORT, LED_PIN, 0);
}

void test_led_write(int on) {
	raw_pin_write(LED_PORT, LED_PIN, on);
}

void test_led_toggle(void) {
	raw_pin_write(LED_PORT, LED_PIN, !((LED_PORT->ODR >> LED_PIN) & 1U));
}
