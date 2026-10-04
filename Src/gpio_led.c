/*
 * gpio_led.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Blinks an external LED on PA6 (open-drain, internal pull-up).
 */

#include "stm32f446xx.h"

#define BLINK_HALF_PERIOD_MS 250U   // LED on 250 ms, off 250 ms

int main(void) {
	GPIO_Handle_t gpioLed;

	gpioLed.Instance = GPIOA;
	gpioLed.Config.PinNumber = 6;
	gpioLed.Config.Mode = GPIO_MODE_OUT;
	gpioLed.Config.OPType = GPIO_OP_TYPE_OD;
	gpioLed.Config.PuPdControl = GPIO_PIN_PU;
	gpioLed.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, DRV_ENABLE);
	GPIO_Init(&gpioLed);

	while (1) {
		GPIO_ToggleOutputPin(GPIOA, 6);
		SYSTICK_DelayMs(BLINK_HALF_PERIOD_MS);
	}

	return 0;
}
