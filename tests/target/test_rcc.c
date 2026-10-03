/*
 * test_rcc.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  RCC driver suite. No wiring.
 *
 *  The RCC driver only reads the clock tree, so the tests program known
 *  prescaler / PLL fields and compare the driver's answer with the value
 *  worked out by hand from RM0390 section 6.
 *
 *  Safety:
 *    - SYSCLK stays on HSI the whole time. Only divider fields are changed.
 *    - PLLCFGR is only written while the PLL is off, so it has no effect on
 *      any running clock.
 *    - Changing HPRE changes HCLK, and the SWO baud rate with it. So nothing is
 *      printed while a test value is active; results are checked after the
 *      original register is restored.
 */

#include "test_harness.h"

#define CFGR_HPRE_MASK    (0xFU << RCC_CFGR_HPRE)
#define CFGR_PPRE1_MASK   (0x7U << RCC_CFGR_PPRE1)
#define CFGR_PPRE2_MASK   (0x7U << RCC_CFGR_PPRE2)

#define PLLCFGR_FIELDS_MASK ((0x3FU  << RCC_PLLCFGR_PLLM) | \
                             (0x1FFU << RCC_PLLCFGR_PLLN) | \
                             (0x3U   << RCC_PLLCFGR_PLLP) | \
                             (0x1U   << RCC_PLLCFGR_PLLSRC))

#define RCC_CR_PLLON      24

static void test_sysclk_after_reset(void) {
	uint32_t sws = (RCC->CFGR >> RCC_CFGR_SWS) & 0x3U;

	if (sws != RCC_SWS_HSI) {
		SKIP("SYSCLK is not HSI, something reconfigured the clocks before the tests");
		return;
	}

	CHECK(RCC_GetSysClkValue() == HSI_VALUE, "SYSCLK on HSI must be 16 MHz");

	if ((RCC->CFGR & (CFGR_HPRE_MASK | CFGR_PPRE1_MASK | CFGR_PPRE2_MASK)) == 0U) {
		CHECK(RCC_GetPCLK1Value() == HSI_VALUE, "PCLK1 with all prescalers /1 must be 16 MHz");
		CHECK(RCC_GetPCLK2Value() == HSI_VALUE, "PCLK2 with all prescalers /1 must be 16 MHz");
	}
}

static void test_bus_prescalers(void) {
	/* hpre / ppre1 / ppre2 are raw field values, see RM0390 RCC_CFGR */
	static const struct {
		uint8_t  hpre, ppre1, ppre2;
		uint32_t pclk1, pclk2;
	} cases[] = {
		{ 0x0, 0x0, 0x0, 16000000U, 16000000U },   /* all /1                          */
		{ 0x8, 0x4, 0x4,  4000000U,  4000000U },   /* AHB /2, APB1 /2, APB2 /2         */
		{ 0x0, 0x5, 0x6,  4000000U,  2000000U },   /* APB1 /4, APB2 /8                 */
		{ 0x0, 0x7, 0x3,  1000000U, 16000000U },   /* APB1 /16, APB2 value 3 still /1  */
		{ 0xB, 0x0, 0x0,  1000000U,  1000000U },   /* AHB /16                          */
		{ 0xC, 0x0, 0x0,   250000U,   250000U },   /* AHB /64 (there is no /32)        */
		{ 0x7, 0x0, 0x0, 16000000U, 16000000U },   /* HPRE < 8 is still /1             */
	};
	enum { N = sizeof(cases) / sizeof(cases[0]) };
	uint32_t got1[N], got2[N];

	if (((RCC->CFGR >> RCC_CFGR_SWS) & 0x3U) != RCC_SWS_HSI) {
		SKIP("SYSCLK is not HSI");
		return;
	}

	test_delay_ms(5);   // let the ITM FIFO drain before HCLK changes

	uint32_t saved = RCC->CFGR;
	for (uint32_t i = 0; i < N; i++) {
		RCC->CFGR = (saved & ~(CFGR_HPRE_MASK | CFGR_PPRE1_MASK | CFGR_PPRE2_MASK))
		          | ((uint32_t)cases[i].hpre  << RCC_CFGR_HPRE)
		          | ((uint32_t)cases[i].ppre1 << RCC_CFGR_PPRE1)
		          | ((uint32_t)cases[i].ppre2 << RCC_CFGR_PPRE2);
		got1[i] = RCC_GetPCLK1Value();
		got2[i] = RCC_GetPCLK2Value();
	}
	RCC->CFGR = saved;
	test_delay_ms(1);

	for (uint32_t i = 0; i < N; i++) {
		if (got1[i] != cases[i].pclk1 || got2[i] != cases[i].pclk2) {
			printf("    case %lu: HPRE=0x%X PPRE1=%u PPRE2=%u -> PCLK1=%lu (exp %lu) PCLK2=%lu (exp %lu)\n",
			       (unsigned long)i, cases[i].hpre, cases[i].ppre1, cases[i].ppre2,
			       (unsigned long)got1[i], (unsigned long)cases[i].pclk1,
			       (unsigned long)got2[i], (unsigned long)cases[i].pclk2);
		}
		CHECK(got1[i] == cases[i].pclk1, "PCLK1 wrong for this prescaler set");
		CHECK(got2[i] == cases[i].pclk2, "PCLK2 wrong for this prescaler set");
	}
}

static void test_pll_output_calc(void) {
	/* PLLP field: 0 -> /2, 1 -> /4, 2 -> /6, 3 -> /8 */
	static const struct {
		uint8_t  src_hse;
		uint8_t  m;
		uint16_t n;
		uint8_t  p_field;
		uint32_t expected;
	} cases[] = {
		{ 0,  8, 180, 0, 180000000U },   /* HSI 16/8*180/2                 */
		{ 0, 16, 336, 1,  84000000U },   /* HSI 16/16*336/4                */
		{ 0,  8, 100, 3,  25000000U },   /* HSI 16/8*100/8                 */
		{ 1,  4, 180, 0, 180000000U },   /* HSE 8/4*180/2 (NUCLEO: 8 MHz)  */
		{ 1,  8, 216, 2,  36000000U },   /* HSE 8/8*216/6                  */
		{ 0,  0, 180, 0,          0U },  /* PLLM=0 invalid: driver returns 0 */
	};
	enum { N = sizeof(cases) / sizeof(cases[0]) };
	uint32_t got[N];

	if (RCC->CR & (1U << RCC_CR_PLLON)) {
		SKIP("PLL is running, PLLCFGR cannot be changed safely");
		return;
	}

	uint32_t saved = RCC->PLLCFGR;
	for (uint32_t i = 0; i < N; i++) {
		RCC->PLLCFGR = (saved & ~PLLCFGR_FIELDS_MASK)
		             | ((uint32_t)cases[i].m       << RCC_PLLCFGR_PLLM)
		             | ((uint32_t)cases[i].n       << RCC_PLLCFGR_PLLN)
		             | ((uint32_t)cases[i].p_field << RCC_PLLCFGR_PLLP)
		             | ((uint32_t)cases[i].src_hse << RCC_PLLCFGR_PLLSRC);
		got[i] = RCC_GetPLLOutputClock();
	}
	RCC->PLLCFGR = saved;

	for (uint32_t i = 0; i < N; i++) {
		if (got[i] != cases[i].expected) {
			printf("    case %lu: got %lu, expected %lu\n",
			       (unsigned long)i, (unsigned long)got[i], (unsigned long)cases[i].expected);
		}
		CHECK(got[i] == cases[i].expected, "PLL output frequency wrong");
	}

	CHECK(RCC->PLLCFGR == saved, "PLLCFGR not restored");
}

void test_suite_rcc(void) {
	test_suite_begin("rcc");

	test_run(test_sysclk_after_reset, "rcc_sysclk_after_reset");
	test_run(test_bus_prescalers,     "rcc_bus_prescalers");
	test_run(test_pll_output_calc,    "rcc_pll_output_calc");
}
