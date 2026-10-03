/*
 * gpio_button_interrupt.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Interrupt: a falling edge on PC5 (button, active low) fires EXTI9_5,
 * and the ISR toggles the LED on PA6.
 */

#include <string.h>
#include "stm32f446xx.h"

static void delay(void) {
	for (volatile uint32_t i = 0; i < 500000/2; i++);
}

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

	GPIO_IRQPriorityConfig(IRQ_NO_EXTI9_5, NVIC_IRQ_PRI15);
	GPIO_IRQInterruptConfig(IRQ_NO_EXTI9_5, DRV_ENABLE);

	while(1);
}

void EXTI9_5_IRQHandler(void) {
	delay();
	GPIO_IRQHandling(5);
	GPIO_ToggleOutputPin(GPIOA, 6);
}
