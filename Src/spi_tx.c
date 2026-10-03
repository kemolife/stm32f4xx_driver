/*
 * spi_tx.c
 *
 *  Created on: 9 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * SPI2 master, software NSS. Sends "HELLO" once after reset on
 * PB15 (MOSI) / PB13 (SCK). Watch it with a logic analyzer.
 */
#include <string.h>
#include "stm32f446xx.h"

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

	// MOSI
	gpioSPI.Config.PinNumber = 15;
	GPIO_Init(&gpioSPI);

	// SCLK
	gpioSPI.Config.PinNumber = 13;
	GPIO_Init(&gpioSPI);
}

static void SPI_ConfigInit(void) {
	SPI_Handle_t SPI_Handle;

	memset(&SPI_Handle, 0, sizeof(SPI_Handle_t));

	SPI_Handle.Instance = SPI2;
	SPI_Handle.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
	SPI_Handle.Config.BusConfig = SPI_BUS_CONFIG_FULL_DUPLEX;   // RXONLY would never transmit
	SPI_Handle.Config.DFF = SPI_DFF_8BITS;
	SPI_Handle.Config.SclkSpeed = SPI_SCLK_SPEED_DIV8;
	SPI_Handle.Config.CPHA = SPI_CPHA_LOW;
	SPI_Handle.Config.CPOL = SPI_CPOL_LOW;
	SPI_Handle.Config.SSM = SPI_SSM_EN;

	SPI_PeriClockControl(SPI2, DRV_ENABLE);
	SPI_Init(&SPI_Handle);
}

int main(void) {
	SPI_GPIO_ConfigInit();
	SPI_ConfigInit();

	SPI_PeripheralControl(SPI2, DRV_ENABLE);

	uint8_t message[] = "HELLO";
	SPI_SendData(SPI2, message, strlen((char*)message));

	// SendData returns when the last byte is queued; wait until it is on the wire
	while (SPI_IsBusy(SPI2));

	SPI_PeripheralControl(SPI2, DRV_DISABLE);

	while(1);
	return 0;
}
