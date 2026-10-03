/*
 * stm32f446xx_systick.h
 *
 *  Created on: 3 Oct 2026
 *  Author: vitaliiantoniuk
 *
 *  1 ms system tick on the Cortex-M4 SysTick timer: a millisecond counter
 *  and a blocking delay. Same idea as HAL_GetTick / HAL_Delay.
 */
#ifndef INC_STM32F446XX_SYSTICK_H_
#define INC_STM32F446XX_SYSTICK_H_

#include "stm32f446xx.h"

/*
 * SysTick CTRL bit positions
 */
#define SYSTICK_CTRL_ENABLE                 0  /* Counter enable                          */
#define SYSTICK_CTRL_TICKINT                1  /* Interrupt on reaching 0                 */
#define SYSTICK_CTRL_CLKSOURCE              2  /* 1 = processor clock (HCLK), 0 = HCLK/8  */
#define SYSTICK_CTRL_COUNTFLAG             16  /* Reached 0 since last read               */

/* SysTick interrupt priority: lowest, so it never delays a peripheral IRQ */
#define SYSTICK_IRQ_PRIORITY               NVIC_IRQ_PRI15

/* ========================================================================== */
/*                       DRIVER API FOR SYSTICK                               */
/* ========================================================================== */

/**
 * @brief  Starts SysTick with a 1 ms period, using the current HCLK
 * @note   RCC_ClockConfig and RCC_DeInit call it again when the clock
 *         changes, so the tick stays 1 ms
 */
void SYSTICK_Init(void);

/**
 * @brief  Milliseconds since SYSTICK_Init. Wraps after ~49 days.
 * @note   Compare times with subtraction, which works across the wrap:
 *         if ((SYSTICK_GetTick() - start) >= timeout_ms) { ... }
 */
uint32_t SYSTICK_GetTick(void);

/**
 * @brief  Waits at least ms milliseconds (at most ms + 1)
 * @note   Starts SysTick if it is not running yet. Do not call it from an
 *         interrupt handler: the tick interrupt has the lowest priority and
 *         cannot run, so the delay would never end
 */
void SYSTICK_DelayMs(uint32_t ms);

#endif /* INC_STM32F446XX_SYSTICK_H_ */
