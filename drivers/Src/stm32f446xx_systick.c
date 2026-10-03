/*
 * stm32f446xx_systick.c
 *
 *  Created on: 3 Oct 2026
 *  Author: vitaliiantoniuk
 */

#include "stm32f446xx.h"

/* Written only by the interrupt, read by everyone else. A 32-bit aligned
 * read is atomic on Cortex-M4, so no interrupt locking is needed. */
static volatile uint32_t tick_ms;

/* Replaces the weak SysTick_Handler from the startup file */
void SysTick_Handler(void) {
	tick_ms++;
}

/******************************************************************************************
 * @fn                 - SYSTICK_Init
 *
 * @brief              - Starts SysTick with a 1 ms period
 *
 * @return             - none
 *
 * @Note               - SysTick counts down from LOAD to 0 at HCLK, then fires and
 *                       reloads. One period is LOAD + 1 cycles, so 1 ms needs
 *                       LOAD = HCLK / 1000 - 1 (16 MHz: 15999, 180 MHz: 179999).
 *                       LOAD is 24 bit, enough for HCLK up to 16.7 GHz
 *
 ******************************************************************************************/
void SYSTICK_Init(void) {
	uint32_t hclk = RCC_GetHCLKValue();

	if (hclk < 1000U) {
		return;   // unknown clock source (RCC_GetHCLKValue returned 0)
	}

	SYSTICK->CTRL = 0U;                       // stop while reprogramming
	SYSTICK->LOAD = (hclk / 1000U) - 1U;
	SYSTICK->VAL  = 0U;                       // any write clears the counter

	/* SysTick is a core exception, not an NVIC line: its priority lives in
	 * SCB SHPR3 [31:24], upper 4 bits used on this core */
	SCB_SHPR3 = (SCB_SHPR3 & ~(0xFFUL << 24)) |
	            ((uint32_t)SYSTICK_IRQ_PRIORITY << (24U + 8U - NO_PR_BITS_IMPLEMENTED));

	SYSTICK->CTRL = (1U << SYSTICK_CTRL_CLKSOURCE) |
	                (1U << SYSTICK_CTRL_TICKINT) |
	                (1U << SYSTICK_CTRL_ENABLE);
}

/******************************************************************************************
 * @fn                 - SYSTICK_GetTick
 *
 * @brief              - Returns milliseconds since SYSTICK_Init
 *
 * @return             - tick count in ms
 *
 * @Note               - wraps after 2^32 ms (~49 days). Use (now - start) for elapsed
 *                       time: unsigned subtraction gives the right result across the wrap
 *
 ******************************************************************************************/
uint32_t SYSTICK_GetTick(void) {
	return tick_ms;
}

/******************************************************************************************
 * @fn                 - SYSTICK_DelayMs
 *
 * @brief              - Busy-waits at least ms milliseconds
 *
 * @param[in]          - delay in ms
 *
 * @return             - none
 *
 * @Note               - one extra tick is added: the first tick may come almost at
 *                       once, so without it DelayMs(1) could return after a few µs.
 *                       Never call it from an interrupt handler
 *
 ******************************************************************************************/
void SYSTICK_DelayMs(uint32_t ms) {
	if (!(SYSTICK->CTRL & (1U << SYSTICK_CTRL_ENABLE))) {
		SYSTICK_Init();
	}

	uint32_t start = tick_ms;
	uint32_t wait = ms;

	if (wait < 0xFFFFFFFFU) {
		wait++;
	}

	while ((tick_ms - start) < wait) {
		/* busy wait */
	}
}
