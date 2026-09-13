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

	gpioSPI.pGPIOx = GPIOB;
	gpioSPI.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
	gpioSPI.GPIO_PinConfig.GPIO_PinAltFunMode = 5;
	gpioSPI.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
	gpioSPI.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NOT_PUPD;
	gpioSPI.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOB, ENABLE);

	// MOSI
	gpioSPI.GPIO_PinConfig.GPIO_PinNumber = 15;
	GPIO_Init(&gpioSPI);

	// SCLK
	gpioSPI.GPIO_PinConfig.GPIO_PinNumber = 13;
	GPIO_Init(&gpioSPI);
}

void SPI_ConfigInit() {
	SPI_Handle_t SPI_Handle;

	memset(&SPI_Handle, 0, sizeof(SPI_Handle_t));

	SPI_Handle.pSPIx = SPI2;
	SPI_Handle.SPI_Config.SPI_DeviceMode = SPI_DEVICE_MODE_MASTER;
	SPI_Handle.SPI_Config.SPI_BusConfig = SPI_BUS_CONFIG_SIMPLEX_RXONLY;
	SPI_Handle.SPI_Config.SPI_DFF = SPI_DFF_8BITS;
	SPI_Handle.SPI_Config.SPI_SclkSpeed = SPI_SCLK_SPEED_DIV8;
	SPI_Handle.SPI_Config.SPI_CPHA = SPI_CPHA_LOW;
	SPI_Handle.SPI_Config.SPI_CPOL = SPI_CPOL_LOW;
	SPI_Handle.SPI_Config.SPI_SSM = SPI_SSM_EN;

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
