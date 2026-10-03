/*
 * test_nvic.c
 *
 *  Created on: 3 Oct 2026
 *  Author: vitaliiantoniuk
 *
 *  NVIC module suite. No wiring.
 *
 *  Interrupts are masked globally (PRIMASK) while lines are enabled here, so
 *  a peripheral that happens to request an interrupt cannot jump into the
 *  Default_Handler loop. The NVIC enable and priority registers still read
 *  back normally while PRIMASK is set.
 */

#include "test_harness.h"

#define NVIC_ICPR_BASE  ((volatile uint32_t *)(NVIC_BASE_ADDR + 0x180UL))

static inline void irq_mask_all(void)   { __asm volatile ("cpsid i" ::: "memory"); }
static inline void irq_unmask_all(void) { __asm volatile ("cpsie i" ::: "memory"); }

static void clear_all_pending(void) {
	for (uint32_t i = 0; i < 4U; i++) {
		NVIC_ICPR_BASE[i] = 0xFFFFFFFFU;
	}
}

static void test_enable_disable_edges(void) {
	/* First and last line of each 32-bit register, and IRQ 96 in the 4th one */
	static const uint8_t irqs[] = { 0, 31, 32, 63, 64, 95, 96 };

	irq_mask_all();
	for (uint32_t i = 0; i < sizeof(irqs); i++) {
		NVIC_IRQInterruptConfig(irqs[i], DRV_ENABLE);
		int on = test_nvic_is_enabled(irqs[i]);
		NVIC_IRQInterruptConfig(irqs[i], DRV_DISABLE);
		int off = !test_nvic_is_enabled(irqs[i]);

		if (!on || !off) {
			printf("    IRQ %u: enable=%d disable=%d\n", irqs[i], on, off);
		}
		CHECK(on,  "IRQ not enabled (wrong ISER register or bit)");
		CHECK(off, "IRQ not disabled (wrong ICER register or bit)");
	}
	clear_all_pending();
	irq_unmask_all();
}

static void test_enable_touches_one_line(void) {
	irq_mask_all();
	NVIC_IRQInterruptConfig(IRQ_NO_SPI2, DRV_ENABLE);           // 36: ISER1 bit 4

	CHECK(!test_nvic_is_enabled(IRQ_NO_SPI1), "enabling IRQ 36 also enabled IRQ 35");
	CHECK(!test_nvic_is_enabled(IRQ_NO_USART1), "enabling IRQ 36 also enabled IRQ 37");
	CHECK(!test_nvic_is_enabled(IRQ_NO_SPI2 - 32U), "enabling IRQ 36 also enabled IRQ 4 (wrong register)");

	NVIC_IRQInterruptConfig(IRQ_NO_SPI2, DRV_DISABLE);
	clear_all_pending();
	irq_unmask_all();
}

static void test_priority_all_slots(void) {
	/* One IRQ in each of the 4 byte slots of a priority register, plus 96 */
	static const uint8_t irqs[] = { 36, 37, 38, 39, 96 };

	for (uint32_t i = 0; i < sizeof(irqs); i++) {
		for (uint32_t prio = 0; prio <= NVIC_PRIORITY_MAX; prio += 5U) {
			NVIC_IRQPriorityConfig(irqs[i], prio);
			uint8_t got = test_nvic_priority(irqs[i]);
			if (got != prio) {
				printf("    IRQ %u: set %lu read %u\n", irqs[i], (unsigned long)prio, got);
			}
			CHECK(got == prio, "priority read back differs");
		}
		NVIC_IRQPriorityConfig(irqs[i], 0);
	}
}

static void test_priority_keeps_neighbours(void) {
	NVIC_IRQPriorityConfig(36, 3);
	NVIC_IRQPriorityConfig(37, 7);
	NVIC_IRQPriorityConfig(38, 11);
	NVIC_IRQPriorityConfig(39, 15);

	NVIC_IRQPriorityConfig(37, 9);   // change one slot only

	CHECK(test_nvic_priority(36) == 3,  "IRQ 36 priority changed by IRQ 37 write");
	CHECK(test_nvic_priority(37) == 9,  "IRQ 37 priority not updated");
	CHECK(test_nvic_priority(38) == 11, "IRQ 38 priority changed by IRQ 37 write");
	CHECK(test_nvic_priority(39) == 15, "IRQ 39 priority changed by IRQ 37 write");

	for (uint8_t irq = 36; irq <= 39; irq++) {
		NVIC_IRQPriorityConfig(irq, 0);
	}
}

static void test_invalid_input_ignored(void) {
	NVIC_IRQPriorityConfig(40, 6);
	NVIC_IRQPriorityConfig(40, NVIC_PRIORITY_MAX + 1U);   // out of range: ignored
	CHECK(test_nvic_priority(40) == 6, "priority 16 was not ignored");

	uint32_t iser3 = NVIC_ISER_BASE_ADDR[3];
	NVIC_IRQInterruptConfig(NVIC_IRQ_COUNT, DRV_ENABLE);      // IRQ 97 does not exist
	CHECK(NVIC_ISER_BASE_ADDR[3] == iser3, "IRQ 97 was not ignored");

	NVIC_IRQPriorityConfig(40, 0);
}

void test_suite_nvic(void) {
	test_suite_begin("nvic");

	test_run(test_enable_disable_edges,      "nvic_enable_disable_edges");
	test_run(test_enable_touches_one_line,   "nvic_enable_touches_one_line");
	test_run(test_priority_all_slots,        "nvic_priority_all_slots");
	test_run(test_priority_keeps_neighbours, "nvic_priority_keeps_neighbours");
	test_run(test_invalid_input_ignored,     "nvic_invalid_input_ignored");
}
