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
 * Maximum frequencies of the F446 (RM0390 6.2, VOS scale 1 with over-drive)
 */
#define RCC_SYSCLK_MAX                       180000000U
#define RCC_PCLK1_MAX                         45000000U  /* APB1 */
#define RCC_PCLK2_MAX                         90000000U  /* APB2 */
#define RCC_OVERDRIVE_ABOVE                  168000000U  /* above this SYSCLK over-drive is needed */

/*
 * RCC CR bit positions
 */
#define RCC_CR_HSION                         0  /* Internal 16 MHz oscillator enable        */
#define RCC_CR_HSIRDY                        1  /* HSI ready                                */
#define RCC_CR_HSEON                        16  /* External oscillator enable               */
#define RCC_CR_HSERDY                       17  /* HSE ready                                */
#define RCC_CR_HSEBYP                       18  /* HSE bypass: external clock, no crystal   */
#define RCC_CR_PLLON                        24  /* Main PLL enable                          */
#define RCC_CR_PLLRDY                       25  /* Main PLL locked                          */

/*
 * RCC CFGR bit positions
 */
#define RCC_CFGR_SW                          0  /* System clock switch          [1:0]   */
#define RCC_CFGR_SWS                         2  /* System clock switch status   [3:2]   */
#define RCC_CFGR_HPRE                        4  /* AHB prescaler                [7:4]   */
#define RCC_CFGR_PPRE1                      10  /* APB1 low-speed prescaler    [12:10]  */
#define RCC_CFGR_PPRE2                      13  /* APB2 high-speed prescaler   [15:13]  */
#define RCC_CFGR_MCO2PRE                    27  /* MCO2 prescaler              [29:27]  */
#define RCC_CFGR_MCO2                       30  /* MCO2 source                 [31:30]  */

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

#define RCC_PLLCFGR_RESET_VALUE     0x24003010U  /* M=16, N=192, P=2, Q=4, R=2, source HSI */

/**
 * @brief Clock tree configuration for RCC_ClockConfig
 *
 *   SYSCLK = (source / PLLM) * PLLN / PLLP
 *   HCLK   = SYSCLK / AHB divider      (CPU, memory, DMA)
 *   PCLK1  = HCLK   / APB1 divider     (max 45 MHz: UART2-5, SPI2/3, I2C)
 *   PCLK2  = HCLK   / APB2 divider     (max 90 MHz: UART1/6, SPI1/4, SYSCFG)
 *
 * Flash wait states, voltage scaling and over-drive are derived from these
 * values by the driver, so they are not part of the config.
 */
typedef struct {
    uint8_t  OscSource;           /* PLL input (@RCC_OscSource)                              */
    uint8_t  PLLM;                /* 2..63, source / PLLM must be 1..2 MHz (2 MHz is best)   */
    uint16_t PLLN;                /* 50..432, (source / PLLM) * PLLN must be 100..432 MHz    */
    uint8_t  PLLP;                /* 2, 4, 6 or 8                                            */
    uint8_t  AHBPrescaler;        /* @RCC_AHBPrescaler                                       */
    uint8_t  APB1Prescaler;       /* @RCC_APBPrescaler                                       */
    uint8_t  APB2Prescaler;       /* @RCC_APBPrescaler                                       */
} RCC_ClockConfig_t;

/*
 * @RCC_OscSource
 */
#define RCC_OSC_HSI                          0  /* internal 16 MHz RC, always available      */
#define RCC_OSC_HSE                          1  /* external crystal on OSC_IN/OSC_OUT        */
#define RCC_OSC_HSE_BYPASS                   2  /* external clock on OSC_IN (NUCLEO: ST-LINK) */

/*
 * @RCC_AHBPrescaler (raw HPRE field values)
 */
#define RCC_AHB_DIV1                         0
#define RCC_AHB_DIV2                         8
#define RCC_AHB_DIV4                         9
#define RCC_AHB_DIV8                        10
#define RCC_AHB_DIV16                       11
#define RCC_AHB_DIV64                       12
#define RCC_AHB_DIV128                      13
#define RCC_AHB_DIV256                      14
#define RCC_AHB_DIV512                      15

/*
 * @RCC_APBPrescaler (raw PPRE1 / PPRE2 field values)
 */
#define RCC_APB_DIV1                         0
#define RCC_APB_DIV2                         4
#define RCC_APB_DIV4                         5
#define RCC_APB_DIV8                         6
#define RCC_APB_DIV16                        7

/*
 * Return values of RCC_ClockConfig / RCC_SetSysClock180MHz
 */
#define RCC_OK                               0
#define RCC_ERROR_CONFIG                     1  /* a value is out of range, nothing changed  */
#define RCC_ERROR_HSE                        2  /* HSE did not start                         */
#define RCC_ERROR_PLL                        3  /* PLL did not lock                          */
#define RCC_ERROR_OVERDRIVE                  4  /* over-drive did not become ready           */
#define RCC_ERROR_FLASH                      5  /* flash wait states not accepted            */
#define RCC_ERROR_SWITCH                     6  /* SYSCLK did not switch                     */

/* ========================================================================== */
/*                       DRIVER API FOR RCC PERIPHERAL                        */
/* ========================================================================== */

/**
 * @brief  Configures oscillator, PLL, bus prescalers, flash wait states and
 *         over-drive, then switches SYSCLK to the PLL
 * @param  pConfig: Desired clock tree
 * @return RCC_OK, or an RCC_ERROR_* code. On any error the MCU is put back on
 *         the reset clock (HSI 16 MHz, all prescalers /1)
 * @note   If SysTick is running it is re-programmed, so 1 tick stays 1 ms
 */
uint8_t RCC_ClockConfig(const RCC_ClockConfig_t *pConfig);

/**
 * @brief  Ready-made setup: SYSCLK 180 MHz, PCLK1 45 MHz, PCLK2 90 MHz
 * @return RCC_OK or an RCC_ERROR_* code
 * @note   Uses the 8 MHz ST-LINK clock (HSE bypass). If it is missing, falls
 *         back to the internal HSI, which is less accurate
 */
uint8_t RCC_SetSysClock180MHz(void);

/**
 * @brief  Puts the clock tree back to its reset state: SYSCLK on HSI 16 MHz,
 *         PLL and HSE off, prescalers /1, 0 flash wait states, no over-drive
 */
void RCC_DeInit(void);

/**
 * @brief  Returns the current AHB clock (HCLK): CPU, SysTick, memory
 * @return HCLK in Hz
 */
uint32_t RCC_GetHCLKValue(void);

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
