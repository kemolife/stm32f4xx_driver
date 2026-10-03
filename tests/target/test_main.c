/*
 * test_main.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  On-target test runner for all drivers. Board: NUCLEO-F446RE, default
 *  clocks (HSI 16 MHz). See tests/README.md for wiring and how to build.
 *
 *  Result:
 *    - SWV ITM console (port 0): one line per test, summary at the end
 *    - g_test_summary in Live Expressions (current_test shows where a hang is)
 *    - LD2: steady ON = all passed (skips allowed), blinking = something failed
 *
 *  Order matters: RCC first (everything uses its clock values), NVIC next
 *  (every driver's IRQ config calls it), then GPIO (every loopback suite uses
 *  GPIO_Init for its pins), then the bus drivers.
 */

#include "test_harness.h"

int main(void) {
	test_harness_init();
	test_led_init();

	printf("\n##### STM32F446 driver tests #####\n");

	test_suite_rcc();
	test_nvic_disable_all_used();

	test_suite_nvic();
	test_nvic_disable_all_used();

	test_suite_gpio();
	test_nvic_disable_all_used();

	test_suite_spi();
	test_nvic_disable_all_used();

	test_suite_i2c();
	test_nvic_disable_all_used();

	test_suite_uart();
	test_nvic_disable_all_used();

	test_print_summary();
	g_test_summary.current_test = "done";
	g_test_summary.done = 1;

	while (1) {
		if (g_test_summary.tests_failed == 0U) {
			test_led_write(1);
		} else {
			test_led_toggle();
			test_delay_ms(250);
		}
	}
}
