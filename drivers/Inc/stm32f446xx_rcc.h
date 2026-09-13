/*
 * stm32f446xx_rcc.h
 *
 *  Created on: 13 Sept 2026
 *  Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_RCC_H_
#define INC_STM32F446XX_RCC_H_

#include "stm32f446xx.h"

/*
 * Oscillator frequencies.
 *
 * HSI is trimmed in silicon and is always 16 MHz on the F4 family.
 *
 * HSE is board dependent, NOT chip dependent. On the NUCLEO-F446RE the HSE
 * input is driven by the 8 MHz MCO output of the on-board ST-LINK (solder
 * bridge default SB54/SB55 ON, X3 crystal not fitted). Override this define
 * if you move the code to a board with its own crystal.
 */
#define HSI_VALUE                           16000000U  /* 16 MHz internal RC */
#ifndef HSE_VALUE
#define HSE_VALUE                            8000000U  /*  8 MHz from ST-LINK MCO */
#endif

/*
 * RCC CFGR bit positions
 */
#define RCC_CFGR_SW                          0  /* System clock switch          [1:0]   */
#define RCC_CFGR_SWS                         2  /* System clock switch status   [3:2]   */
#define RCC_CFGR_HPRE                        4  /* AHB prescaler                [7:4]   */
#define RCC_CFGR_PPRE1                      10  /* APB1 low-speed prescaler    [12:10]  */
#define RCC_CFGR_PPRE2                      13  /* APB2 high-speed prescaler   [15:13]  */

/*
 * RCC CFGR SWS values (which clock is actually driving SYSCLK right now)
 */
#define RCC_SWS_HSI                          0
#define RCC_SWS_HSE                          1
#define RCC_SWS_PLL_P                        2
#define RCC_SWS_PLL_R                        3

/*
 * RCC PLLCFGR bit positions
 */
#define RCC_PLLCFGR_PLLM                     0  /* Division factor for the PLL input    [5:0]   */
#define RCC_PLLCFGR_PLLN                     6  /* Main PLL multiplication factor      [14:6]   */
#define RCC_PLLCFGR_PLLP                    16  /* Main PLL division factor for SYSCLK [17:16]  */
#define RCC_PLLCFGR_PLLSRC                  22  /* PLL source: 0 = HSI, 1 = HSE                 */
#define RCC_PLLCFGR_PLLR                    28  /* Main PLL division factor for I2S/SAI[30:28]  */

/* ========================================================================== */
/*                       DRIVER API FOR RCC PERIPHERAL                        */
/* ========================================================================== */

/**
 * @brief  Returns the frequency the PLL is currently producing on its P output
 * @return PLL output clock in Hz
 */
uint32_t RCC_GetPLLOutputClock(void);

/**
 * @brief  Returns the current SYSCLK frequency by inspecting the active source
 * @return System clock in Hz
 */
uint32_t RCC_GetSysClkValue(void);

/**
 * @brief  Returns the current APB1 peripheral clock frequency
 * @return PCLK1 in Hz
 */
uint32_t RCC_GetPCLK1Value(void);

/**
 * @brief  Returns the current APB2 peripheral clock frequency
 * @return PCLK2 in Hz
 */
uint32_t RCC_GetPCLK2Value(void);

#endif /* INC_STM32F446XX_RCC_H_ */
