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
#include "string.h"
#include "stm32f446xx.h"

// ==========================================
// 💡 CONFIGURABLE COMMAND MACROS
// Change these values to test different requests
// ==========================================
#define CMD_GET_STATUS    0x10  // Request Pico system status
#define CMD_GET_SENSOR    0x20  // Request latest sensor reading
#define CMD_GET_HELLO     0x55  // Request a text greeting ("HELLO")
#define CMD_LED_ON        0xA1  // Tell Pico to turn ON its LED
#define CMD_LED_OFF       0xA2  // Tell Pico to turn OFF its LED

// Active command used in the loop (Swap this to try different commands!)
#define CURRENT_COMMAND   CMD_GET_HELLO

void delay() {
	for (uint32_t i = 0; i < 500000; i++);
}

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

	// MOSI -> Pico GPIO12 (SPI1 RX)
	gpioSPI.Config.PinNumber = 15;
	GPIO_Init(&gpioSPI);

	// MISO -> Pico GPIO11 (SPI1 TX)
	gpioSPI.Config.PinNumber = 14;
	GPIO_Init(&gpioSPI);

	// SCLK -> Pico GPIO10
	gpioSPI.Config.PinNumber = 13;
	GPIO_Init(&gpioSPI);

	// NSS -> Pico GPIO13. Hardware-driven via SSOE.
	gpioSPI.Config.PinNumber = 12;
	GPIO_Init(&gpioSPI);
}

void SPI_ConfigInit() {
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

	SPI_PeriClockControl(SPI2, ENABLE);
	SPI_Init(&SPI_Handle);
}

void GPIO_ButtonInit() {
	GPIO_Handle_t gpioButton;

	memset(&gpioButton, 0, sizeof(GPIO_Handle_t));

	gpioButton.Instance = GPIOC;
	gpioButton.Config.PinNumber = 13;
	gpioButton.Config.Mode = GPIO_MODE_IN;
	gpioButton.Config.PuPdControl = GPIO_PIN_PU;
	gpioButton.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, ENABLE);
	GPIO_Init(&gpioButton);
}

// Helper function to send a command and instantly read a response byte
uint8_t SPI_TransferByte(SPI_RegDef_t *pSPIx, uint8_t transmitByte) {
    uint8_t receiveByte = 0;

    // Call your custom library API for single-byte exchange, or direct registers:
    SPI_SendData(pSPIx, &transmitByte, 1);
    SPI_ReceiveData(pSPIx, &receiveByte, 1);

    return receiveByte;
}

int main() {
	SPI_GPIO_ConfigInit();
	SPI_ConfigInit();
	GPIO_ButtonInit();

	// Move the Peripheral Enable OUTSIDE the loop so it stays active
	SPI_PeripheralControl(SPI2, ENABLE);

	while(1) {
		// 1. Wait for button press
		while (GPIO_ReadFromInputPin(GPIOC, 13) == 1);
		delay();

		// 2. Transmit the macro command configured at the top of the file
		SPI_TransferByte(SPI2, CURRENT_COMMAND);

		// Small buffer loading window for the Pico slave
		for(volatile int i = 0; i < 50; i++);

		// 3. Clock out a dummy byte to receive data length from Pico
		uint8_t rxLen = SPI_TransferByte(SPI2, 0x00);

		// 4. Collect payload if the specific macro command expects a return string
		if (rxLen > 0 && rxLen < 100) {
			uint8_t rxBuffer[100] = {0};

			for(uint8_t i = 0; i < rxLen; i++) {
				rxBuffer[i] = SPI_TransferByte(SPI2, 0x00);
			}
			// rxBuffer now holds the relevant data requested by CURRENT_COMMAND
		}

		// 5. Complete cycle management
		while ( SPI_IsBusy(SPI2) );
		while (GPIO_ReadFromInputPin(GPIOC, 13) == 0);
		delay();
	}
}
