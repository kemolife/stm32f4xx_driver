/*
 * rcc_clock_180mhz.c
 *
 *  Created on: 3 Oct 2026
 *      Author: vitaliiantoniuk
 *
 * Switches the MCU from the 16 MHz reset clock to 180 MHz, then blinks LD2
 * with SYSTICK_DelayMs.
 *
 *   LD2 (PA5) 1 Hz blink   - running at 180 MHz, SysTick correct
 *                            (count 10 blinks against a stopwatch: 10 s)
 *   LD2 fast blink (10 Hz) - clock setup failed, still on HSI 16 MHz
 *   PC9 (MCO2)             - SYSCLK / 5 = 36 MHz, for a scope or
 *                            frequency counter (CN10 pin 1)
 *
 * SWV printf: in the debug configuration set the core clock to 180 MHz
 * (Debugger > Serial Wire Viewer), otherwise the console shows garbage
 * after the switch, because SWO is clocked from HCLK.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f446xx.h"

#define LED_PIN         5    // LD2 on PA5
#define MCO2_PIN        9    // PC9, AF0 = MCO2

#define ITM_STIM0       (*(volatile uint32_t*)0xE0000000)
#define ITM_TER         (*(volatile uint32_t*)0xE0000E00)
#define DEMCR           (*(volatile uint32_t*)0xE000EDFC)
#define DEMCR_TRCENA    (1UL << 24)

static void ITM_SendChar(char ch) {
	if (!(DEMCR & DEMCR_TRCENA)) { return; }   // trace not enabled by debugger
	if (!(ITM_TER & 1UL))        { return; }   // stimulus port 0 disabled

	while (ITM_STIM0 == 0);                    // wait for port to accept a byte
	*(volatile uint8_t*)&ITM_STIM0 = (uint8_t)ch;
}

int _write(int file, char *ptr, int len) {
	(void)file;
	for (int i = 0; i < len; i++) {
		ITM_SendChar(ptr[i]);
	}
	return len;
}

static void GPIO_LedInit(void) {
	GPIO_Handle_t gpioLed;

	memset(&gpioLed, 0, sizeof(GPIO_Handle_t));

	gpioLed.Instance = GPIOA;
	gpioLed.Config.PinNumber = LED_PIN;
	gpioLed.Config.Mode = GPIO_MODE_OUT;
	gpioLed.Config.OPType = GPIO_OP_TYPE_PP;
	gpioLed.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioLed.Config.Speed = GPIO_SPEED_LOW;

	GPIO_PeriClockControl(GPIOA, DRV_ENABLE);
	GPIO_Init(&gpioLed);
}

/* Puts SYSCLK / 5 on PC9, so the real frequency can be measured */
static void MCO2_Init(void) {
	GPIO_Handle_t gpioMco;

	memset(&gpioMco, 0, sizeof(GPIO_Handle_t));

	gpioMco.Instance = GPIOC;
	gpioMco.Config.PinNumber = MCO2_PIN;
	gpioMco.Config.Mode = GPIO_MODE_ALTFN;
	gpioMco.Config.AltFunMode = 0;                  // AF0 = MCO2
	gpioMco.Config.OPType = GPIO_OP_TYPE_PP;
	gpioMco.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioMco.Config.Speed = GPIO_SPEED_HIGH;          // 36 MHz edge needs the fast driver

	GPIO_PeriClockControl(GPIOC, DRV_ENABLE);
	GPIO_Init(&gpioMco);

	/* MCO2 source 00 = SYSCLK, prescaler 111 = /5 */
	RCC->CFGR &= ~((0x3U << RCC_CFGR_MCO2) | (0x7U << RCC_CFGR_MCO2PRE));
	RCC->CFGR |= (0x7U << RCC_CFGR_MCO2PRE);
}

int main(void) {
	GPIO_LedInit();

	uint8_t status = RCC_SetSysClock180MHz();

	/* After a failure the clock is back at 16 MHz, so MCO2 shows 3.2 MHz */
	MCO2_Init();

	printf("clock setup: %s (code %u)\n", (status == RCC_OK) ? "OK" : "FAILED", status);
	printf("SYSCLK %lu Hz, HCLK %lu Hz, PCLK1 %lu Hz, PCLK2 %lu Hz\n",
	       (unsigned long)RCC_GetSysClkValue(), (unsigned long)RCC_GetHCLKValue(),
	       (unsigned long)RCC_GetPCLK1Value(), (unsigned long)RCC_GetPCLK2Value());

	uint32_t half_period_ms = (status == RCC_OK) ? 500U : 50U;

	while (1) {
		GPIO_ToggleOutputPin(GPIOA, LED_PIN);
		SYSTICK_DelayMs(half_period_ms);
	}
}
