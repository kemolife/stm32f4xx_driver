/*
 * stm32f446xx_nvic.c
 *
 *  Created on: 3 Oct 2026
 *  Author: vitaliiantoniuk
 */

#include "stm32f446xx.h"

/******************************************************************************************
 * @fn                 - NVIC_IRQInterruptConfig
 *
 * @brief              - Enables or disables one interrupt line in the NVIC
 *
 * @param[in]          - IRQ number (0 .. 96)
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - ISER/ICER are write-1-to-act: writing 0 to the other bits has no
 *                       effect, so a plain assignment is correct and a read-modify-write
 *                       would be wrong. Each register covers 32 lines, so IRQ 96 needs
 *                       the fourth register (ISER3/ICER3)
 *
 ******************************************************************************************/
void NVIC_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi) {
	if (IRQNumber >= NVIC_IRQ_COUNT) {
		return;
	}

	uint32_t reg = IRQNumber / 32U;
	uint32_t bit = 1UL << (IRQNumber % 32U);

	if (EnorDi == ENABLE) {
		NVIC_ISER_BASE_ADDR[reg] = bit;
	} else {
		NVIC_ICER_BASE_ADDR[reg] = bit;
	}
}

/******************************************************************************************
 * @fn                 - NVIC_IRQPriorityConfig
 *
 * @brief              - Sets the priority level of one interrupt line
 *
 * @param[in]          - IRQ number (0 .. 96)
 * @param[in]          - priority 0 (most urgent) .. 15 (least urgent)
 *
 * @return             - none
 *
 * @Note               - Four IRQs share one 32-bit priority register, one byte each. Only
 *                       the top NO_PR_BITS_IMPLEMENTED bits of each byte exist on this
 *                       core, so the value is shifted into the upper half of its byte
 *
 ******************************************************************************************/
void NVIC_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	if (IRQNumber >= NVIC_IRQ_COUNT || IRQPriority > NVIC_PRIORITY_MAX) {
		return;
	}

	uint32_t iprx = IRQNumber / 4U;
	uint32_t byte_shift = (IRQNumber % 4U) * 8U;
	uint32_t shift_amount = byte_shift + (8U - NO_PR_BITS_IMPLEMENTED);

	uint32_t temp = NVIC_PR_BASE_ADDR[iprx];
	temp &= ~(0xFFUL << byte_shift);
	temp |= (IRQPriority << shift_amount);
	NVIC_PR_BASE_ADDR[iprx] = temp;
}
