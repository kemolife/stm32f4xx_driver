/*
 * gpio_button.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Polling: toggles the LED on PA6 while the button on PC5 (active low,
 * internal pull-up) is pressed.
 */

#include "stm32f446xx.h"

#define DEBOUNCE_MS          200U   // ignore contact bounce after a press

int main(void) {
	GPIO_Handle_t gpioLed, gpioButton;

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
	gpioButton.Config.Mode = GPIO_MODE_IN;
	gpioButton.Config.PuPdControl = GPIO_PIN_PU;
	gpioButton.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, DRV_ENABLE);
	GPIO_Init(&gpioButton);

	while (1) {
		if (GPIO_ReadFromInputPin(GPIOC, 5) == 0) {
			SYSTICK_DelayMs(DEBOUNCE_MS);
			GPIO_ToggleOutputPin(GPIOA, 6);
		}
	}

	return 0;
}
