/*
 * stm32f446xx_rcc.c
 *
 *  Created on: 13 Sept 2026
 *      Author: vitaliiantoniuk
 */

#include "stm32f446xx.h"

/*
 * AHB prescaler lookup (RCC_CFGR HPRE, bits [7:4]).
 *
 * The field is 4 bits, but only values 8..15 divide anything: the MSB acts as
 * an "enable". Any value with bit 3 clear (0..7) means "AHB = SYSCLK / 1".
 * The table below is indexed by (HPRE - 8), so the caller must range-check
 * first. Note the gap between 16 and 64: divider 32 does not exist.
 */
static const uint16_t AHB_PreScaler[8] = { 2, 4, 8, 16, 64, 128, 256, 512 };

/*
 * APB prescaler lookup (RCC_CFGR PPRE1/PPRE2, 3 bits each).
 *
 * Same idea: values 0..3 mean "divide by 1", values 4..7 select a real
 * divider. Indexed by (PPRE - 4).
 */
static const uint8_t APB_PreScaler[4] = { 2, 4, 8, 16 };

/*
 * Upper bound for every "wait until the hardware is ready" loop in this file.
 * SysTick may not run yet (or runs from the clock being changed), so a loop
 * count is used instead of milliseconds. At 16 MHz this is about 0.3 s, far
 * longer than the HSE start-up (~2 ms) or the PLL lock (~0.1 ms).
 */
#define RCC_TIMEOUT_LOOPS       1000000U

/* Flash needs one extra wait state per 30 MHz of HCLK at 2.7..3.6 V
 * (RM0390 table 5). The NUCLEO board runs at 3.3 V. */
#define FLASH_HZ_PER_WAIT_STATE 30000000U

#define CFGR_SW_MASK            (0x3U << RCC_CFGR_SW)
#define CFGR_SWS_MASK           (0x3U << RCC_CFGR_SWS)
#define CFGR_PRESCALER_MASK     ((0xFU << RCC_CFGR_HPRE) | (0x7U << RCC_CFGR_PPRE1) | (0x7U << RCC_CFGR_PPRE2))

static uint32_t ahb_divider(uint32_t hpre) {
	return (hpre < 8U) ? 1U : AHB_PreScaler[hpre - 8U];
}

static uint32_t apb_divider(uint32_t ppre) {
	return (ppre < 4U) ? 1U : APB_PreScaler[ppre - 4U];
}

/* Waits until all bits in mask are set (set = 1) or all are clear (set = 0).
 * Returns 1 on success, 0 on timeout. */
static int wait_bits(volatile uint32_t *reg, uint32_t mask, int set) {
	for (uint32_t i = 0; i < RCC_TIMEOUT_LOOPS; i++) {
		uint32_t v = *reg & mask;
		if (set ? (v == mask) : (v == 0U)) {
			return 1;
		}
	}
	return 0;
}

/* Makes the HSI the system clock. The HSI cannot fail, so this is the safe
 * place to stand while the PLL or HSE are being changed. */
static int switch_to_hsi(void) {
	RCC->CR |= (1U << RCC_CR_HSION);
	if (!wait_bits(&RCC->CR, 1U << RCC_CR_HSIRDY, 1)) {
		return 0;
	}

	RCC->CFGR = (RCC->CFGR & ~CFGR_SW_MASK) | ((uint32_t)RCC_SWS_HSI << RCC_CFGR_SW);

	/* SW is a request. SWS shows when the hardware has really switched. */
	return wait_bits(&RCC->CFGR, CFGR_SWS_MASK, 0);   // SWS = 00: HSI
}

/******************************************************************************************
 * @fn                 - RCC_GetPLLOutputClock
 *
 * @brief              - Computes the frequency the main PLL is delivering on its P output
 *
 * @return             - PLL output clock in Hz
 *
 * @Note               - The F446 main PLL is a two-stage design:
 *
 *                         VCO     = PLLSRC / PLLM * PLLN
 *                         PLLCLK  = VCO / PLLP
 *
 *                       PLLM divides the input down to the 1-2 MHz the VCO
 *                       expects, PLLN multiplies that up into the VCO range,
 *                       PLLP scales it back down for SYSCLK.
 *
 ******************************************************************************************/
uint32_t RCC_GetPLLOutputClock(void)
{
	uint32_t pllcfgr = RCC->PLLCFGR;

	// PLLSRC (bit 22) picks which oscillator feeds the PLL input divider
	uint32_t pllsource = (pllcfgr >> RCC_PLLCFGR_PLLSRC) & 0x1U;
	uint32_t inputclock = (pllsource == 1U) ? HSE_VALUE : HSI_VALUE;

	// PLLM [5:0]: input divider, legal range 2..63. Guard against a zero
	// divide in case the register was never programmed.
	uint32_t pllm = (pllcfgr >> RCC_PLLCFGR_PLLM) & 0x3FU;
	if (pllm == 0U) {
		return 0U;
	}

	// PLLN [14:6]: VCO multiplier, legal range 50..432
	uint32_t plln = (pllcfgr >> RCC_PLLCFGR_PLLN) & 0x1FFU;

	// PLLP [17:16] is encoded, not literal: 00 -> 2, 01 -> 4, 10 -> 6, 11 -> 8
	uint32_t pllp = (((pllcfgr >> RCC_PLLCFGR_PLLP) & 0x3U) + 1U) * 2U;

	return (inputclock / pllm) * plln / pllp;
}

/******************************************************************************************
 * @fn                 - RCC_GetSysClkValue
 *
 * @brief              - Reports which clock is currently driving SYSCLK and its frequency
 *
 * @return             - System clock in Hz
 *
 * @Note               - Reads SWS (status), not SW (request). SW is what software
 *                       asked for; SWS is what the hardware actually switched to
 *                       once the new source became stable.
 *
 ******************************************************************************************/
uint32_t RCC_GetSysClkValue(void)
{
	uint8_t clksrc = (uint8_t)((RCC->CFGR >> RCC_CFGR_SWS) & 0x3U);

	switch (clksrc) {
	case RCC_SWS_HSI:
		return HSI_VALUE;
	case RCC_SWS_HSE:
		return HSE_VALUE;
	case RCC_SWS_PLL_P:
		return RCC_GetPLLOutputClock();
	default:
		// SWS = 11 selects the PLL R output, which this driver does not
		// configure. Returning 0 makes the mistake loud instead of silently
		// producing a wrong baud rate downstream.
		return 0U;
	}
}

/******************************************************************************************
 * @fn                 - RCC_GetHCLKValue
 *
 * @brief              - Calculates the AHB clock (HCLK): CPU, SysTick, memory, DMA
 *
 * @return             - HCLK in Hz
 *
 * @Note               - SYSCLK --[HPRE]--> HCLK
 *
 ******************************************************************************************/
uint32_t RCC_GetHCLKValue(void)
{
	uint32_t hpre = (RCC->CFGR >> RCC_CFGR_HPRE) & 0xFU;
	return RCC_GetSysClkValue() / ahb_divider(hpre);
}

/******************************************************************************************
 * @fn                 - RCC_GetPCLK1Value
 *
 * @brief              - Calculates the clock frequency currently feeding APB1 peripherals
 *
 * @return             - PCLK1 in Hz
 *
 * @Note               - The clock tree is a chain of dividers:
 *
 *                         SYSCLK --[HPRE]--> HCLK (AHB) --[PPRE1]--> PCLK1 (APB1)
 *
 *                       I2C1/2/3, SPI2/3, USART2/3 and UART4/5 all live on APB1,
 *                       so their timing registers (I2C CR2 FREQ, CCR, TRISE) must
 *                       be programmed from this value, not from a hardcoded constant.
 *
 *                       APB1 is limited to 45 MHz on the F446.
 *
 ******************************************************************************************/
uint32_t RCC_GetPCLK1Value(void)
{
	uint32_t ppre1 = (RCC->CFGR >> RCC_CFGR_PPRE1) & 0x7U;
	return RCC_GetHCLKValue() / apb_divider(ppre1);
}

/******************************************************************************************
 * @fn                 - RCC_GetPCLK2Value
 *
 * @brief              - Calculates the clock frequency currently feeding APB2 peripherals
 *
 * @return             - PCLK2 in Hz
 *
 * @Note               - Identical chain to PCLK1 but through PPRE2 [15:13].
 *                       SPI1/SPI4, USART1/USART6 and SYSCFG sit on APB2, which
 *                       runs up to 90 MHz on the F446.
 *
 ******************************************************************************************/
uint32_t RCC_GetPCLK2Value(void)
{
	uint32_t ppre2 = (RCC->CFGR >> RCC_CFGR_PPRE2) & 0x7U;
	return RCC_GetHCLKValue() / apb_divider(ppre2);
}

/******************************************************************************************
 * @fn                 - RCC_ClockConfig
 *
 * @brief              - Configures the whole clock tree and switches SYSCLK to the PLL
 *
 * @param[in]          - pointer to the desired clock configuration
 *
 * @return             - RCC_OK or an RCC_ERROR_* code
 *
 * @Note               - Sequence from RM0390 5.1.4 (over-drive) and 6.2:
 *                         1. check every value before touching any register
 *                         2. stand on HSI, stop the PLL
 *                         3. start the HSE if it is the PLL source
 *                         4. regulator to scale 1
 *                         5. program and start the PLL
 *                         6. over-drive on, if SYSCLK > 168 MHz
 *                         7. flash wait states for the new HCLK
 *                         8. bus prescalers
 *                         9. wait for PLL lock, switch SYSCLK, check SWS
 *                       Steps 7 and 8 come before the switch: the CPU must never
 *                       run faster than the flash or the buses allow, not even
 *                       for one cycle
 *
 ******************************************************************************************/
uint8_t RCC_ClockConfig(const RCC_ClockConfig_t *pConfig)
{
	/* ---- 1. Check every value. Nothing is changed if one is wrong. ---- */
	uint32_t source_hz = (pConfig->OscSource == RCC_OSC_HSI) ? HSI_VALUE : HSE_VALUE;

	if (pConfig->OscSource > RCC_OSC_HSE_BYPASS ||
	    pConfig->PLLM < 2U || pConfig->PLLM > 63U ||
	    pConfig->PLLN < 50U || pConfig->PLLN > 432U ||
	    (pConfig->PLLP != 2U && pConfig->PLLP != 4U && pConfig->PLLP != 6U && pConfig->PLLP != 8U) ||
	    pConfig->AHBPrescaler > RCC_AHB_DIV512 ||
	    pConfig->APB1Prescaler > RCC_APB_DIV16 ||
	    pConfig->APB2Prescaler > RCC_APB_DIV16) {
		return RCC_ERROR_CONFIG;
	}

	uint32_t vco_in  = source_hz / pConfig->PLLM;          // must be ~1..2 MHz
	uint32_t vco_out = vco_in * pConfig->PLLN;             // must be 100..432 MHz
	uint32_t sysclk  = vco_out / pConfig->PLLP;
	uint32_t hclk    = sysclk / ahb_divider(pConfig->AHBPrescaler);
	uint32_t pclk1   = hclk / apb_divider(pConfig->APB1Prescaler);
	uint32_t pclk2   = hclk / apb_divider(pConfig->APB2Prescaler);

	if (vco_in < 950000U || vco_in > 2100000U ||
	    vco_out < 100000000U || vco_out > 432000000U ||
	    sysclk > RCC_SYSCLK_MAX || pclk1 > RCC_PCLK1_MAX || pclk2 > RCC_PCLK2_MAX) {
		return RCC_ERROR_CONFIG;
	}

	/* ---- 2. Run from HSI while the PLL is changed. A running PLL cannot be
	 *         reprogrammed, and SYSCLK must not depend on it meanwhile. ---- */
	if (!switch_to_hsi()) {
		RCC_DeInit();
		return RCC_ERROR_SWITCH;
	}
	RCC->CR &= ~(1U << RCC_CR_PLLON);
	if (!wait_bits(&RCC->CR, 1U << RCC_CR_PLLRDY, 0)) {
		RCC_DeInit();
		return RCC_ERROR_PLL;
	}

	/* ---- 3. HSE. HSEBYP can only be changed while the HSE is off. ---- */
	if (pConfig->OscSource != RCC_OSC_HSI) {
		RCC->CR &= ~(1U << RCC_CR_HSEON);
		wait_bits(&RCC->CR, 1U << RCC_CR_HSERDY, 0);

		if (pConfig->OscSource == RCC_OSC_HSE_BYPASS) {
			RCC->CR |= (1U << RCC_CR_HSEBYP);
		} else {
			RCC->CR &= ~(1U << RCC_CR_HSEBYP);
		}

		RCC->CR |= (1U << RCC_CR_HSEON);
		if (!wait_bits(&RCC->CR, 1U << RCC_CR_HSERDY, 1)) {
			RCC_DeInit();   // no clock on OSC_IN: switch the HSE off again
			return RCC_ERROR_HSE;
		}
	}

	/* ---- 4. Regulator scale 1 (highest performance). VOS may only change
	 *         while the PLL is off; it takes effect when the PLL starts. ---- */
	PWR_PCLK_EN();
	(void)RCC->APB1ENR;   // read back: the clock needs 2 cycles before PWR is usable
	PWR->CR = (PWR->CR & ~(0x3U << PWR_CR_VOS)) | (0x3U << PWR_CR_VOS);

	/* ---- 5. PLL. Q and R (USB/SAI/I2S outputs) keep their current values. ---- */
	uint32_t pllcfgr = RCC->PLLCFGR;
	pllcfgr &= ~((0x3FU  << RCC_PLLCFGR_PLLM) |
	             (0x1FFU << RCC_PLLCFGR_PLLN) |
	             (0x3U   << RCC_PLLCFGR_PLLP) |
	             (0x1U   << RCC_PLLCFGR_PLLSRC));
	pllcfgr |= ((uint32_t)pConfig->PLLM << RCC_PLLCFGR_PLLM);
	pllcfgr |= ((uint32_t)pConfig->PLLN << RCC_PLLCFGR_PLLN);
	pllcfgr |= ((uint32_t)(pConfig->PLLP / 2U - 1U) << RCC_PLLCFGR_PLLP);   // 2,4,6,8 -> 0,1,2,3
	if (pConfig->OscSource != RCC_OSC_HSI) {
		pllcfgr |= (1U << RCC_PLLCFGR_PLLSRC);
	}
	RCC->PLLCFGR = pllcfgr;

	RCC->CR |= (1U << RCC_CR_PLLON);

	/* ---- 6. Over-drive: needed above 168 MHz. Enabled while the PLL locks,
	 *         as in RM0390 5.1.4. ---- */
	if (sysclk > RCC_OVERDRIVE_ABOVE) {
		PWR->CR |= (1U << PWR_CR_ODEN);
		if (!wait_bits(&PWR->CSR, 1U << PWR_CSR_ODRDY, 1)) {
			RCC_DeInit();
			return RCC_ERROR_OVERDRIVE;
		}
		PWR->CR |= (1U << PWR_CR_ODSWEN);
		if (!wait_bits(&PWR->CSR, 1U << PWR_CSR_ODSWRDY, 1)) {
			RCC_DeInit();
			return RCC_ERROR_OVERDRIVE;
		}
	}

	/* ---- 7. Flash wait states for the new HCLK, plus prefetch and caches.
	 *         Read back: the new latency must be active before the switch. ---- */
	uint32_t latency = (hclk - 1U) / FLASH_HZ_PER_WAIT_STATE;   // 180 MHz -> 5
	FLASH->ACR = (latency << FLASH_ACR_LATENCY) |
	             (1U << FLASH_ACR_PRFTEN) | (1U << FLASH_ACR_ICEN) | (1U << FLASH_ACR_DCEN);
	if (((FLASH->ACR >> FLASH_ACR_LATENCY) & 0xFU) != latency) {
		RCC_DeInit();
		return RCC_ERROR_FLASH;
	}

	/* ---- 8. Bus prescalers. Set while still on HSI, so APB1/APB2 never
	 *         exceed their limits after the switch. ---- */
	RCC->CFGR = (RCC->CFGR & ~CFGR_PRESCALER_MASK) |
	            ((uint32_t)pConfig->AHBPrescaler  << RCC_CFGR_HPRE) |
	            ((uint32_t)pConfig->APB1Prescaler << RCC_CFGR_PPRE1) |
	            ((uint32_t)pConfig->APB2Prescaler << RCC_CFGR_PPRE2);

	/* ---- 9. Wait for lock, switch, check the status field ---- */
	if (!wait_bits(&RCC->CR, 1U << RCC_CR_PLLRDY, 1)) {
		RCC_DeInit();
		return RCC_ERROR_PLL;
	}

	RCC->CFGR = (RCC->CFGR & ~CFGR_SW_MASK) | ((uint32_t)RCC_SWS_PLL_P << RCC_CFGR_SW);
	if (!wait_bits(&RCC->CFGR, (uint32_t)RCC_SWS_PLL_P << RCC_CFGR_SWS, 1)) {
		RCC_DeInit();
		return RCC_ERROR_SWITCH;
	}

	/* SysTick counts HCLK cycles: re-program it so 1 tick is still 1 ms */
	if (SYSTICK->CTRL & (1U << SYSTICK_CTRL_ENABLE)) {
		SYSTICK_Init();
	}

	return RCC_OK;
}

/******************************************************************************************
 * @fn                 - RCC_SetSysClock180MHz
 *
 * @brief              - Ready-made clock setup: SYSCLK 180 MHz, PCLK1 45 MHz, PCLK2 90 MHz
 *
 * @return             - RCC_OK or an RCC_ERROR_* code
 *
 * @Note               - NUCLEO-F446RE: the ST-LINK feeds 8 MHz into OSC_IN (HSE bypass).
 *                       If that clock is missing (other board, solder bridge open),
 *                       the HSI is used instead. Both give exactly 180 MHz; HSE is
 *                       more accurate (crystal on the ST-LINK vs. RC oscillator)
 *
 ******************************************************************************************/
uint8_t RCC_SetSysClock180MHz(void)
{
	RCC_ClockConfig_t cfg = {
		.OscSource     = RCC_OSC_HSE_BYPASS,
		.PLLM          = 4,              // 8 MHz / 4   = 2 MHz VCO input
		.PLLN          = 180,            // 2 MHz * 180 = 360 MHz VCO output
		.PLLP          = 2,              // 360 MHz / 2 = 180 MHz SYSCLK
		.AHBPrescaler  = RCC_AHB_DIV1,   // HCLK  180 MHz
		.APB1Prescaler = RCC_APB_DIV4,   // PCLK1  45 MHz
		.APB2Prescaler = RCC_APB_DIV2,   // PCLK2  90 MHz
	};

	uint8_t status = RCC_ClockConfig(&cfg);

	if (status == RCC_ERROR_HSE) {
		cfg.OscSource = RCC_OSC_HSI;
		cfg.PLLM = 8;                    // 16 MHz / 8 = 2 MHz, rest unchanged
		status = RCC_ClockConfig(&cfg);
	}

	return status;
}

/******************************************************************************************
 * @fn                 - RCC_DeInit
 *
 * @brief              - Puts the clock tree back to its reset state
 *
 * @return             - none
 *
 * @Note               - Order matters when slowing down: first switch SYSCLK to HSI,
 *                       then remove the over-drive and the extra flash wait states.
 *                       The other way round the CPU would briefly run too fast for
 *                       the flash
 *
 ******************************************************************************************/
void RCC_DeInit(void)
{
	/* 1. SYSCLK on HSI, prescalers /1, MCO outputs to reset value */
	switch_to_hsi();
	RCC->CFGR = 0U;
	wait_bits(&RCC->CFGR, CFGR_SWS_MASK, 0);

	/* 2. PLL off and back to its reset settings */
	RCC->CR &= ~(1U << RCC_CR_PLLON);
	wait_bits(&RCC->CR, 1U << RCC_CR_PLLRDY, 0);
	RCC->PLLCFGR = RCC_PLLCFGR_RESET_VALUE;

	/* 3. Over-drive off (RM0390 5.1.4: only after SYSCLK left the PLL) */
	PWR_PCLK_EN();
	(void)RCC->APB1ENR;
	PWR->CR &= ~((1U << PWR_CR_ODEN) | (1U << PWR_CR_ODSWEN));
	wait_bits(&PWR->CSR, 1U << PWR_CSR_ODSWRDY, 0);

	/* 4. Flash: 0 wait states, prefetch and caches off (reset value) */
	FLASH->ACR = 0U;

	/* 5. HSE off, then bypass off (HSEBYP is only writable with HSE off) */
	RCC->CR &= ~(1U << RCC_CR_HSEON);
	wait_bits(&RCC->CR, 1U << RCC_CR_HSERDY, 0);
	RCC->CR &= ~(1U << RCC_CR_HSEBYP);

	/* SysTick counts HCLK cycles: re-program it so 1 tick is still 1 ms */
	if (SYSTICK->CTRL & (1U << SYSTICK_CTRL_ENABLE)) {
		SYSTICK_Init();
	}
}
