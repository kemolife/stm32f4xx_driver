/*
 * stm32f446xx_nvic.h
 *
 *  Created on: 3 Oct 2026
 *  Author: vitaliiantoniuk
 *
 *  Interrupt controller (NVIC) helpers shared by every peripheral driver.
 *  The GPIO, SPI, I2C and UART *_IRQInterruptConfig / *_IRQPriorityConfig
 *  functions call these, so the NVIC logic exists in one place only.
 */
#ifndef INC_STM32F446XX_NVIC_H_
#define INC_STM32F446XX_NVIC_H_

#include "stm32f446xx.h"

/* STM32F446 has 97 external interrupt lines: IRQ 0 (WWDG) .. 96 (FMPI2C1_ER) */
#define NVIC_IRQ_COUNT          97U

/* Highest number accepted by NVIC_IRQPriorityConfig (4 bits: 0 = most urgent) */
#define NVIC_PRIORITY_MAX       ((1U << NO_PR_BITS_IMPLEMENTED) - 1U)

/* ========================================================================== */
/*                       DRIVER API FOR NVIC                                  */
/* ========================================================================== */

/**
 * @brief  Enables or disables one interrupt line in the NVIC
 * @param  IRQNumber: IRQ number, one of the IRQ_NO_* macros (0 .. 96)
 * @param  EnorDi: ENABLE or DISABLE
 * @note   Numbers outside 0 .. 96 are ignored
 */
void NVIC_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi);

/**
 * @brief  Sets the priority of one interrupt line
 * @param  IRQNumber: IRQ number, one of the IRQ_NO_* macros (0 .. 96)
 * @param  IRQPriority: 0 (most urgent) .. 15 (least urgent), NVIC_IRQ_PRI* macros
 * @note   Numbers outside 0 .. 96 and priorities above 15 are ignored
 */
void NVIC_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);

#endif /* INC_STM32F446XX_NVIC_H_ */
