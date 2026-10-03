/*
 * test_gpio.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  GPIO driver suite.
 *
 *  Pins (all free on the NUCLEO-F446RE with nothing plugged in):
 *    PC8   register and pull-up/pull-down tests   (CN10 pin 2)  - leave unconnected
 *    PC3   alternate function low  (AFR[0])      (CN7 pin 37)
 *    PC12  alternate function high (AFR[1])      (CN7 pin 3)
 *    PC6   EXTI line 6 by software trigger       (CN10 pin 4)
 *    GPIOH only the pull/speed fields are touched for DeInit (PH0/PH1 are OSC pins)
 *
 *  [LOOP] jumper: PC10 (CN7 pin 1) <-> PC11 (CN7 pin 2).
 *  One jumper cap fits, the two pins are side by side.
 *
 *  Never DeInit GPIOA or GPIOB: that resets PA13/PA14 (SWD) and PB3 (SWO)
 *  and the debugger connection is lost.
 */

#include <string.h>
#include "test_harness.h"

#define REG_PIN        8    /* PC8  */
#define AF_LOW_PIN     3    /* PC3  */
#define AF_HIGH_PIN    12   /* PC12 */
#define EXTI_SW_PIN    6    /* PC6  */
#define LOOP_OUT_PIN   10   /* PC10 */
#define LOOP_IN_PIN    11   /* PC11 */

static volatile uint8_t exti6_count;
static volatile uint8_t exti11_count;

void EXTI9_5_IRQHandler(void) {
	if (EXTI->PR & (1U << EXTI_SW_PIN)) {
		exti6_count++;
	}
	GPIO_IRQHandling(EXTI_SW_PIN);
}

void EXTI15_10_IRQHandler(void) {
	if (EXTI->PR & (1U << LOOP_IN_PIN)) {
		exti11_count++;
	}
	GPIO_IRQHandling(LOOP_IN_PIN);
}

static void pin_config(GPIO_Handle_t *h, GPIO_RegDef_t *port, uint8_t pin, uint8_t mode,
                       uint8_t optype, uint8_t speed, uint8_t pupd, uint8_t af) {
	memset(h, 0, sizeof(*h));
	h->Instance = port;
	h->Config.PinNumber = pin;
	h->Config.Mode = mode;
	h->Config.OPType = optype;
	h->Config.Speed = speed;
	h->Config.PuPdControl = pupd;
	h->Config.AltFunMode = af;
}

static uint32_t field2(uint32_t reg, uint8_t pin) { return (reg >> (2U * pin)) & 0x3U; }
static uint32_t field4(uint32_t reg, uint8_t pos) { return (reg >> (4U * pos)) & 0xFU; }

/* Puts a GPIOC pin back to its reset state (input, no pull, PP, low speed) */
static void pin_reset_raw(uint8_t pin) {
	GPIOC->MODER   &= ~(0x3U << (2U * pin));
	GPIOC->PUPDR   &= ~(0x3U << (2U * pin));
	GPIOC->OSPEEDR &= ~(0x3U << (2U * pin));
	GPIOC->OTYPER  &= ~(1U << pin);
}

/* ========================================================================== */
/*   No wiring                                                                */
/* ========================================================================== */

static void test_clock_control(void) {
	GPIO_PeriClockControl(GPIOC, ENABLE);
	CHECK(RCC->AHB1ENR & (1U << 2), "GPIOC clock not enabled (AHB1ENR bit 2)");

	GPIO_PeriClockControl(GPIOH, ENABLE);
	CHECK(RCC->AHB1ENR & (1U << 7), "GPIOH clock not enabled (AHB1ENR bit 7)");

	GPIO_PeriClockControl(GPIOH, DISABLE);
	CHECK(!(RCC->AHB1ENR & (1U << 7)), "GPIOH clock not disabled");
}

static void test_init_output_fields(void) {
	GPIO_Handle_t h;

	GPIO_PeriClockControl(GPIOC, ENABLE);
	pin_reset_raw(REG_PIN);

	/* Neighbours must not change: snapshot everything except PC8's fields */
	uint32_t moder_other = GPIOC->MODER & ~(0x3U << (2U * REG_PIN));
	uint32_t pupdr_other = GPIOC->PUPDR & ~(0x3U << (2U * REG_PIN));

	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_OUT, GPIO_OP_TYPE_OD, GPIO_SPEED_HIGH, GPIO_PIN_PD, 0);
	GPIO_Init(&h);

	CHECK(field2(GPIOC->MODER, REG_PIN) == GPIO_MODE_OUT,     "MODER not output (01)");
	CHECK(GPIOC->OTYPER & (1U << REG_PIN),                     "OTYPER not open-drain");
	CHECK(field2(GPIOC->OSPEEDR, REG_PIN) == GPIO_SPEED_HIGH,  "OSPEEDR not high (11)");
	CHECK(field2(GPIOC->PUPDR, REG_PIN) == GPIO_PIN_PD,        "PUPDR not pull-down (10)");

	CHECK((GPIOC->MODER & ~(0x3U << (2U * REG_PIN))) == moder_other, "GPIO_Init changed MODER of other pins");
	CHECK((GPIOC->PUPDR & ~(0x3U << (2U * REG_PIN))) == pupdr_other, "GPIO_Init changed PUPDR of other pins");

	pin_reset_raw(REG_PIN);
}

static void test_init_reconfigure_overwrites(void) {
	GPIO_Handle_t h;

	/* First every field at a non-zero value... */
	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_ANALOG, GPIO_OP_TYPE_OD, GPIO_SPEED_HIGH, GPIO_PIN_PD, 0);
	GPIO_Init(&h);

	/* ...then all back to zero values. Stale bits would show up here. */
	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_IN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&h);

	CHECK(field2(GPIOC->MODER, REG_PIN) == 0U,   "MODER keeps old bits after re-init");
	CHECK(!(GPIOC->OTYPER & (1U << REG_PIN)),     "OTYPER keeps old bit after re-init");
	CHECK(field2(GPIOC->OSPEEDR, REG_PIN) == 0U, "OSPEEDR keeps old bits after re-init");
	CHECK(field2(GPIOC->PUPDR, REG_PIN) == 0U,   "PUPDR keeps old bits after re-init");
}

static void test_init_alternate_function(void) {
	GPIO_Handle_t h;

	pin_config(&h, GPIOC, AF_LOW_PIN, GPIO_MODE_ALTFN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 5);
	GPIO_Init(&h);
	CHECK(field2(GPIOC->MODER, AF_LOW_PIN) == GPIO_MODE_ALTFN, "PC3 MODER not alternate (10)");
	CHECK(field4(GPIOC->AFR[0], AF_LOW_PIN) == 5U,             "PC3 AFRL not AF5");

	pin_config(&h, GPIOC, AF_HIGH_PIN, GPIO_MODE_ALTFN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 9);
	GPIO_Init(&h);
	CHECK(field4(GPIOC->AFR[1], AF_HIGH_PIN - 8U) == 9U, "PC12 AFRH not AF9");
	CHECK(field4(GPIOC->AFR[0], AF_LOW_PIN) == 5U,       "setting PC12 changed PC3 AF");

	/* Re-init with another AF must replace, not OR */
	pin_config(&h, GPIOC, AF_HIGH_PIN, GPIO_MODE_ALTFN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 6);
	GPIO_Init(&h);
	CHECK(field4(GPIOC->AFR[1], AF_HIGH_PIN - 8U) == 6U, "PC12 AFRH not replaced (OR of old and new?)");

	GPIOC->AFR[0] &= ~(0xFU << (4U * AF_LOW_PIN));
	GPIOC->AFR[1] &= ~(0xFU << (4U * (AF_HIGH_PIN - 8U)));
	pin_reset_raw(AF_LOW_PIN);
	pin_reset_raw(AF_HIGH_PIN);
}

static void test_output_write_toggle(void) {
	GPIO_Handle_t h;

	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_OUT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&h);

	uint32_t odr_other = GPIOC->ODR & ~(1U << REG_PIN);

	/* IDR reads the real pin level, so this proves the pad follows ODR */
	GPIO_WriteToOutputPin(GPIOC, REG_PIN, GPIO_PIN_SET);
	CHECK(GPIOC->ODR & (1U << REG_PIN),  "ODR bit not set");
	CHECK(GPIOC->IDR & (1U << REG_PIN),  "pin level not high (IDR)");

	GPIO_WriteToOutputPin(GPIOC, REG_PIN, GPIO_PIN_RESET);
	CHECK(!(GPIOC->ODR & (1U << REG_PIN)), "ODR bit not cleared");
	CHECK(!(GPIOC->IDR & (1U << REG_PIN)), "pin level not low (IDR)");

	GPIO_ToggleOutputPin(GPIOC, REG_PIN);
	CHECK(GPIOC->ODR & (1U << REG_PIN), "toggle from low did not give high");
	GPIO_ToggleOutputPin(GPIOC, REG_PIN);
	CHECK(!(GPIOC->ODR & (1U << REG_PIN)), "toggle from high did not give low");

	CHECK((GPIOC->ODR & ~(1U << REG_PIN)) == odr_other, "pin write changed other ODR bits");

	CHECK(GPIO_ReadFromInputPin(GPIOC, REG_PIN) == 0U, "ReadFromInputPin disagrees with IDR");
	CHECK(GPIO_ReadFromInputPort(GPIOC) == (uint16_t)GPIOC->IDR, "ReadFromInputPort disagrees with IDR");

	/* Port write: only PC8 is an output on GPIOC right now, so this is safe */
	uint32_t saved = GPIOC->ODR;
	GPIO_WriteToOutputPort(GPIOC, 0x0100U);
	CHECK(GPIOC->ODR == 0x0100U, "WriteToOutputPort value not in ODR");
	GPIOC->ODR = saved;

	pin_reset_raw(REG_PIN);
}

static void test_input_pull_up_down(void) {
	GPIO_Handle_t h;

	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_IN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PU, 0);
	GPIO_Init(&h);
	test_delay_ms(1);
	CHECK(GPIO_ReadFromInputPin(GPIOC, REG_PIN) == 1U, "PC8 with pull-up reads 0 (is something connected to PC8?)");

	pin_config(&h, GPIOC, REG_PIN, GPIO_MODE_IN, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PD, 0);
	GPIO_Init(&h);
	test_delay_ms(1);
	CHECK(GPIO_ReadFromInputPin(GPIOC, REG_PIN) == 0U, "PC8 with pull-down reads 1 (is something connected to PC8?)");

	pin_reset_raw(REG_PIN);
}

static void test_deinit(void) {
	/* GPIOH reset value of every register is 0. Only PUPDR/OSPEEDR are
	 * written: PH0/PH1 are oscillator pins and must never be outputs. */
	GPIO_PeriClockControl(GPIOH, ENABLE);
	GPIOH->PUPDR = 0x5U;
	GPIOH->OSPEEDR = 0xFU;

	GPIO_DeInit(GPIOH);

	CHECK(GPIOH->PUPDR == 0U,   "GPIOH PUPDR not reset");
	CHECK(GPIOH->OSPEEDR == 0U, "GPIOH OSPEEDR not reset");

	GPIO_PeriClockControl(GPIOH, DISABLE);
}

static void test_exti_config(void) {
	GPIO_Handle_t h;

	/* Precondition: the pin was an output before. An interrupt pin must end
	 * up as an input, or the EXTI line would watch our own output. */
	pin_config(&h, GPIOC, EXTI_SW_PIN, GPIO_MODE_OUT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&h);

	pin_config(&h, GPIOC, EXTI_SW_PIN, GPIO_MODE_IT_FT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PU, 0);
	GPIO_Init(&h);

	CHECK(field2(GPIOC->MODER, EXTI_SW_PIN) == GPIO_MODE_IN, "IT mode pin is not input (MODER must be 00)");
	CHECK(EXTI->FTSR & (1U << EXTI_SW_PIN),    "IT_FT: FTSR bit not set");
	CHECK(!(EXTI->RTSR & (1U << EXTI_SW_PIN)), "IT_FT: RTSR bit set");
	CHECK(EXTI->IMR & (1U << EXTI_SW_PIN),     "EXTI line not unmasked (IMR)");
	CHECK(RCC->APB2ENR & (1U << 14),           "SYSCFG clock not enabled");
	CHECK(field4(SYSCFG->EXTICR[1], EXTI_SW_PIN % 4U) == 2U, "EXTICR2 does not select port C for line 6");

	pin_config(&h, GPIOC, EXTI_SW_PIN, GPIO_MODE_IT_RT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PU, 0);
	GPIO_Init(&h);
	CHECK(EXTI->RTSR & (1U << EXTI_SW_PIN),    "IT_RT: RTSR bit not set");
	CHECK(!(EXTI->FTSR & (1U << EXTI_SW_PIN)), "IT_RT: FTSR bit still set");

	pin_config(&h, GPIOC, EXTI_SW_PIN, GPIO_MODE_IT_RFT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PU, 0);
	GPIO_Init(&h);
	CHECK((EXTI->RTSR & (1U << EXTI_SW_PIN)) && (EXTI->FTSR & (1U << EXTI_SW_PIN)),
	      "IT_RFT: RTSR and FTSR not both set");

	EXTI->IMR &= ~(1U << EXTI_SW_PIN);
	pin_reset_raw(EXTI_SW_PIN);
}

static void test_nvic_config(void) {
	static const uint8_t irqs[] = { IRQ_NO_EXTI0, IRQ_NO_EXTI4, IRQ_NO_EXTI9_5, IRQ_NO_EXTI15_10 };

	for (uint32_t i = 0; i < sizeof(irqs); i++) {
		GPIO_IRQInterruptConfig(irqs[i], ENABLE);
		CHECK(test_nvic_is_enabled(irqs[i]), "EXTI IRQ not enabled in NVIC");

		GPIO_IRQPriorityConfig(irqs[i], NVIC_IRQ_PRI13);
		CHECK(test_nvic_priority(irqs[i]) == NVIC_IRQ_PRI13, "EXTI IRQ priority not 13");

		GPIO_IRQPriorityConfig(irqs[i], NVIC_IRQ_PRI2);
		CHECK(test_nvic_priority(irqs[i]) == NVIC_IRQ_PRI2, "EXTI IRQ priority not replaced by 2");

		GPIO_IRQInterruptConfig(irqs[i], DISABLE);
		CHECK(!test_nvic_is_enabled(irqs[i]), "EXTI IRQ not disabled in NVIC");
	}
}

static void test_exti_software_irq(void) {
	GPIO_Handle_t h;

	pin_config(&h, GPIOC, EXTI_SW_PIN, GPIO_MODE_IT_FT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_PIN_PU, 0);
	GPIO_Init(&h);

	exti6_count = 0;
	EXTI->PR = (1U << EXTI_SW_PIN);   // start clean
	GPIO_IRQPriorityConfig(IRQ_NO_EXTI9_5, NVIC_IRQ_PRI13);
	GPIO_IRQInterruptConfig(IRQ_NO_EXTI9_5, ENABLE);

	/* SWIER sets the pending bit as if an edge came in, no wire needed */
	EXTI->SWIER = (1U << EXTI_SW_PIN);

	CHECK(test_wait_count(&exti6_count, 1, TEST_TIMEOUT_MS), "EXTI9_5 IRQ did not fire");
	CHECK(!(EXTI->PR & (1U << EXTI_SW_PIN)), "GPIO_IRQHandling did not clear the pending bit");
	test_delay_ms(1);
	CHECK(exti6_count == 1, "IRQ fired more than once (pending bit not cleared?)");

	GPIO_IRQInterruptConfig(IRQ_NO_EXTI9_5, DISABLE);
	EXTI->IMR &= ~(1U << EXTI_SW_PIN);
	pin_reset_raw(EXTI_SW_PIN);
}

/* ========================================================================== */
/*   [LOOP] PC10 -> PC11                                                      */
/* ========================================================================== */

static int loop_wire_present(void) {
	if (!test_wire_connected(GPIOC, LOOP_OUT_PIN, GPIOC, LOOP_IN_PIN)) {
		SKIP("jumper PC10 <-> PC11 (CN7 pin 1-2) missing");
		return 0;
	}
	return 1;
}

static void test_loop_output_to_input(void) {
	GPIO_Handle_t out, in;

	if (!loop_wire_present()) {
		return;
	}

	pin_config(&out, GPIOC, LOOP_OUT_PIN, GPIO_MODE_OUT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	pin_config(&in,  GPIOC, LOOP_IN_PIN,  GPIO_MODE_IN,  GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&out);
	GPIO_Init(&in);

	for (int i = 0; i < 4; i++) {
		uint8_t level = (uint8_t)(i & 1);
		GPIO_WriteToOutputPin(GPIOC, LOOP_OUT_PIN, level);
		test_delay_ms(1);
		CHECK(GPIO_ReadFromInputPin(GPIOC, LOOP_IN_PIN) == level, "input does not follow output");
	}

	pin_reset_raw(LOOP_OUT_PIN);
	pin_reset_raw(LOOP_IN_PIN);
}

/* Drives one low -> high -> low pulse and returns how many IRQs came */
static uint8_t pulse_and_count(void) {
	GPIO_WriteToOutputPin(GPIOC, LOOP_OUT_PIN, GPIO_PIN_RESET);
	test_delay_ms(1);
	exti11_count = 0;

	GPIO_WriteToOutputPin(GPIOC, LOOP_OUT_PIN, GPIO_PIN_SET);
	test_delay_ms(1);
	GPIO_WriteToOutputPin(GPIOC, LOOP_OUT_PIN, GPIO_PIN_RESET);
	test_delay_ms(1);

	return exti11_count;
}

static void test_loop_edge_interrupts(void) {
	GPIO_Handle_t out, in;

	if (!loop_wire_present()) {
		return;
	}

	pin_config(&out, GPIOC, LOOP_OUT_PIN, GPIO_MODE_OUT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&out);

	GPIO_IRQPriorityConfig(IRQ_NO_EXTI15_10, NVIC_IRQ_PRI13);
	GPIO_IRQInterruptConfig(IRQ_NO_EXTI15_10, ENABLE);

	pin_config(&in, GPIOC, LOOP_IN_PIN, GPIO_MODE_IT_RT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&in);
	CHECK(pulse_and_count() == 1, "IT_RT: expected 1 IRQ per pulse (rising edge only)");

	pin_config(&in, GPIOC, LOOP_IN_PIN, GPIO_MODE_IT_FT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&in);
	CHECK(pulse_and_count() == 1, "IT_FT: expected 1 IRQ per pulse (falling edge only)");

	pin_config(&in, GPIOC, LOOP_IN_PIN, GPIO_MODE_IT_RFT, GPIO_OP_TYPE_PP, GPIO_SPEED_LOW, GPIO_NOT_PUPD, 0);
	GPIO_Init(&in);
	CHECK(pulse_and_count() == 2, "IT_RFT: expected 2 IRQs per pulse (both edges)");

	GPIO_IRQInterruptConfig(IRQ_NO_EXTI15_10, DISABLE);
	EXTI->IMR &= ~(1U << LOOP_IN_PIN);
	EXTI->RTSR &= ~(1U << LOOP_IN_PIN);
	EXTI->FTSR &= ~(1U << LOOP_IN_PIN);
	pin_reset_raw(LOOP_OUT_PIN);
	pin_reset_raw(LOOP_IN_PIN);
}

void test_suite_gpio(void) {
	test_suite_begin("gpio");

	test_run(test_clock_control,              "gpio_clock_control");
	test_run(test_init_output_fields,         "gpio_init_output_fields");
	test_run(test_init_reconfigure_overwrites,"gpio_init_reconfigure_overwrites");
	test_run(test_init_alternate_function,    "gpio_init_alternate_function");
	test_run(test_output_write_toggle,        "gpio_output_write_toggle");
	test_run(test_input_pull_up_down,         "gpio_input_pull_up_down");
	test_run(test_deinit,                     "gpio_deinit");
	test_run(test_exti_config,                "gpio_exti_config");
	test_run(test_nvic_config,                "gpio_nvic_config");
	test_run(test_exti_software_irq,          "gpio_exti_software_irq");

	test_run(test_loop_output_to_input,       "gpio_loop_output_to_input");
	test_run(test_loop_edge_interrupts,       "gpio_loop_edge_interrupts");
}
