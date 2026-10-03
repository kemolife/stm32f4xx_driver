/*
 * spi_tx.c
 *
 *  Created on: 9 Sept 2026
 *      Author: vitaliiantoniuk
 */
#include "string.h"
#include "stm32f446xx.h"

void SPI_GPIO_ConfigInit() {
	GPIO_Handle_t gpioSPI;

	memset(&gpioSPI, 0, sizeof(GPIO_Handle_t));

	gpioSPI.Instance = GPIOB;
	gpioSPI.Config.Mode = GPIO_MODE_ALTFN;
	gpioSPI.Config.AltFunMode = 5;
	gpioSPI.Config.OPType = GPIO_OP_TYPE_PP;
	gpioSPI.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioSPI.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOB, ENABLE);

	// MOSI
	gpioSPI.Config.PinNumber = 15;
	GPIO_Init(&gpioSPI);

	// SCLK
	gpioSPI.Config.PinNumber = 13;
	GPIO_Init(&gpioSPI);
}

void SPI_ConfigInit() {
	SPI_Handle_t SPI_Handle;

	memset(&SPI_Handle, 0, sizeof(SPI_Handle_t));

	SPI_Handle.Instance = SPI2;
	SPI_Handle.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
	SPI_Handle.Config.BusConfig = SPI_BUS_CONFIG_SIMPLEX_RXONLY;
	SPI_Handle.Config.DFF = SPI_DFF_8BITS;
	SPI_Handle.Config.SclkSpeed = SPI_SCLK_SPEED_DIV8;
	SPI_Handle.Config.CPHA = SPI_CPHA_LOW;
	SPI_Handle.Config.CPOL = SPI_CPOL_LOW;
	SPI_Handle.Config.SSM = SPI_SSM_EN;

	SPI_PeriClockControl(SPI2, ENABLE);
	SPI_Init(&SPI_Handle);
}

int main() {
	SPI_GPIO_ConfigInit();
	SPI_ConfigInit();

	SPI_PeripheralControl(SPI2, ENABLE);

	uint8_t message[] = "HELLO";
	SPI_SendData(SPI2, message, strlen((char*)message));

	SPI_PeripheralControl(SPI2, DISABLE);

	while(1);
	return 0;
}
