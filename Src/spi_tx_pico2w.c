/*
 * spi_tx.c
 *
 *  Created on: 9 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * SPI2 master. On each button press, sends a length byte followed by an
 * ASCII payload to a Raspberry Pi Pico 2 W running as an SPI slave.
 *
 * Two settings here are not obvious and were established by measurement,
 * not by datasheet reading. Both must hold or the link fails:
 *
 *   CPHA=1. With CPHA=0 the Pico's PL022 latches on the NSS falling edge and
 *   freezes its shift register, so it accepts exactly one byte per assertion
 *   -- and none at all if NSS was already low when the Pico booted.
 *
 *   500 kHz. At 8 MHz the PL022 slave returns every byte shifted left by one
 *   bit, even though its documented ceiling is clk_peri/12 = 12.5 MHz. The
 *   payload is six bytes per button press, so there is nothing to gain by
 *   pushing the clock toward that limit.
 *
 * With CPHA=1 correct, hardware NSS (SSOE) is sufficient; NSS may sit low
 * permanently and the slave still receives multi-byte bursts. Manual CS
 * pulsing was needed only as a workaround while CPHA was still wrong.
 */
#include <string.h>
#include "stm32f446xx.h"

#define SPI_TIMEOUT_MS       10U    // a few bytes at 500 kHz take < 1 ms

#define DEBOUNCE_MS          200U   // ignore contact bounce after a press

static void SPI_GPIO_ConfigInit(void) {
	GPIO_Handle_t gpioSPI;

	memset(&gpioSPI, 0, sizeof(GPIO_Handle_t));

	gpioSPI.Instance = GPIOB;
	gpioSPI.Config.Mode = GPIO_MODE_ALTFN;
	gpioSPI.Config.AltFunMode = 5;
	gpioSPI.Config.OPType = GPIO_OP_TYPE_PP;
	gpioSPI.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioSPI.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOB, DRV_ENABLE);

	// MOSI -> Pico GPIO12 (SPI1 RX)
	gpioSPI.Config.PinNumber = 15;
	GPIO_Init(&gpioSPI);

	// SCLK -> Pico GPIO10
	gpioSPI.Config.PinNumber = 13;
	GPIO_Init(&gpioSPI);

	// NSS -> Pico GPIO13. Hardware-driven via SSOE.
	gpioSPI.Config.PinNumber = 12;
	GPIO_Init(&gpioSPI);
}

static void SPI_ConfigInit(void) {
	SPI_Handle_t SPI_Handle;

	memset(&SPI_Handle, 0, sizeof(SPI_Handle_t));

	SPI_Handle.Instance = SPI2;
	SPI_Handle.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
	SPI_Handle.Config.BusConfig = SPI_BUS_CONFIG_FULL_DUPLEX;
	SPI_Handle.Config.DFF = SPI_DFF_8BITS;
	SPI_Handle.Config.SclkSpeed = SPI_SCLK_SPEED_DIV32;  // 16 MHz / 32 = 500 kHz
	SPI_Handle.Config.CPOL = SPI_CPOL_LOW;
	SPI_Handle.Config.CPHA = SPI_CPHA_HIGH;  // SPH=1: allows a multi-byte
	                                                 // burst under one CS assertion
	SPI_Handle.Config.SSM = SPI_SSM_DI;      // hardware NSS (SSOE)

	SPI_PeriClockControl(SPI2, DRV_ENABLE);
	SPI_Init(&SPI_Handle);
}

static void GPIO_ButtonInit(void) {
	GPIO_Handle_t gpioButton;

	memset(&gpioButton, 0, sizeof(GPIO_Handle_t));

	gpioButton.Instance = GPIOC;
	gpioButton.Config.PinNumber = 13;
	gpioButton.Config.Mode = GPIO_MODE_IN;
	gpioButton.Config.PuPdControl = GPIO_PIN_PU;
	gpioButton.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, DRV_ENABLE);
	GPIO_Init(&gpioButton);
}

int main(void) {
	SPI_GPIO_ConfigInit();
	SPI_ConfigInit();
	GPIO_ButtonInit();

	// Move the Peripheral Enable OUTSIDE the loop so it stays active
	SPI_PeripheralControl(SPI2, DRV_ENABLE);

	while(1) {
		// 1. Wait for button press (assuming active-LOW button)
		while (GPIO_ReadFromInputPin(GPIOC, 13) == 1);
		SYSTICK_DelayMs(DEBOUNCE_MS); // Simple debounce delay

		uint8_t message[] = "HELLO";
		uint8_t dataLen = strlen((char *)message);

		// 2. Send length and payload
		SPI_SendData(SPI2, &dataLen, 1, SPI_TIMEOUT_MS);
		SPI_SendData(SPI2, message, dataLen, SPI_TIMEOUT_MS);

		// 3. Wait for transmission to physically finish completely
		while ( SPI_IsBusy(SPI2) );

		// 3. Wait for user to RELEASE the button before looping again
		while (GPIO_ReadFromInputPin(GPIOC, 13) == 0);
		SYSTICK_DelayMs(DEBOUNCE_MS);
	}
}
