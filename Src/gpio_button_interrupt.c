/*
 * gpio_button_interrupt.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Interrupt: a falling edge on PC5 (button, active low) fires EXTI9_5,
 * and the ISR toggles the LED on PA6.
 *
 * Debounce without blocking: a mechanical button gives several edges per
 * press. The ISR ignores every edge that comes less than DEBOUNCE_MS after
 * the last accepted one. Waiting inside the ISR is not possible:
 * SYSTICK_DelayMs needs the SysTick interrupt, which cannot run while this
 * handler (same lowest priority) is active.
 */

#include <string.h>
#include "stm32f446xx.h"

#define DEBOUNCE_MS          200U   // ignore contact bounce after a press

static uint32_t last_press_ms;

int main(void) {
	GPIO_Handle_t gpioLed, gpioButton;

	// 1. Clear the memory completely
	memset(&gpioLed, 0, sizeof(GPIO_Handle_t));
	memset(&gpioButton, 0, sizeof(GPIO_Handle_t));

	gpioLed.Instance = GPIOA;
	gpioLed.Config.PinNumber = 6;
	gpioLed.Config.Mode = GPIO_MODE_OUT;
	gpioLed.Config.OPType = GPIO_OP_TYPE_PP;
	gpioLed.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioLed.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, DRV_ENABLE);
	GPIO_Init(&gpioLed);

	gpioButton.Instance = GPIOC;
	gpioButton.Config.PinNumber = 5;
	gpioButton.Config.Mode = GPIO_MODE_IT_FT;
	gpioButton.Config.PuPdControl = GPIO_PIN_PU;
	gpioButton.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, DRV_ENABLE);
	GPIO_Init(&gpioButton);

	SYSTICK_Init();   // the debounce reads the ms tick

	GPIO_IRQPriorityConfig(IRQ_NO_EXTI9_5, NVIC_IRQ_PRI15);
	GPIO_IRQInterruptConfig(IRQ_NO_EXTI9_5, DRV_ENABLE);

	while(1);
}

void EXTI9_5_IRQHandler(void) {
	GPIO_IRQHandling(5);   // always clear the pending bit, also for ignored edges

	uint32_t now = SYSTICK_GetTick();
	if ((now - last_press_ms) >= DEBOUNCE_MS) {
		last_press_ms = now;
		GPIO_ToggleOutputPin(GPIOA, 6);
	}
}
