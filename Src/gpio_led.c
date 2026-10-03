/*
 * gpio_led.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Blinks an external LED on PA6 (open-drain, internal pull-up).
 */

#include "stm32f446xx.h"

static void delay(void) {
	for (volatile uint32_t i = 0; i < 500000; i++);
}

int main(void) {
	GPIO_Handle_t gpioLed;

	gpioLed.Instance = GPIOA;
	gpioLed.Config.PinNumber = 6;
	gpioLed.Config.Mode = GPIO_MODE_OUT;
	gpioLed.Config.OPType = GPIO_OP_TYPE_OD;
	gpioLed.Config.PuPdControl = GPIO_PIN_PU;
	gpioLed.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, ENABLE);
	GPIO_Init(&gpioLed);

	while (1) {
		GPIO_ToggleOutputPin(GPIOA, 6);
		delay();
	}

	return 0;
}
