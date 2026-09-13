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
	// 1. Start from whatever is actually driving the system clock
	uint32_t systemclk = RCC_GetSysClkValue();

	// 2. AHB prescaler: HPRE [7:4]. Values below 8 mean no division at all,
	//    so only indexes 8..15 hit the lookup table.
	uint8_t hpre = (uint8_t)((RCC->CFGR >> RCC_CFGR_HPRE) & 0xFU);
	uint16_t ahbp = (hpre < 8U) ? 1U : AHB_PreScaler[hpre - 8U];

	// 3. APB1 prescaler: PPRE1 [12:10]. Same encoding, values below 4 mean /1.
	uint8_t ppre1 = (uint8_t)((RCC->CFGR >> RCC_CFGR_PPRE1) & 0x7U);
	uint8_t apb1p = (ppre1 < 4U) ? 1U : APB_PreScaler[ppre1 - 4U];

	// 4. Apply both dividers in order
	return (systemclk / ahbp) / apb1p;
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
	uint32_t systemclk = RCC_GetSysClkValue();

	uint8_t hpre = (uint8_t)((RCC->CFGR >> RCC_CFGR_HPRE) & 0xFU);
	uint16_t ahbp = (hpre < 8U) ? 1U : AHB_PreScaler[hpre - 8U];

	uint8_t ppre2 = (uint8_t)((RCC->CFGR >> RCC_CFGR_PPRE2) & 0x7U);
	uint8_t apb2p = (ppre2 < 4U) ? 1U : APB_PreScaler[ppre2 - 4U];

	return (systemclk / ahbp) / apb2p;
}
