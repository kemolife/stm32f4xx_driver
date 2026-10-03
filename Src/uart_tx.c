/*
 * uart_tx.c
 *
 *  Created on: 28 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * USART2 transmitter, polling. On each press of the user button (B1, PC13)
 * a numbered text line is sent to the PC.
 *
 * No wiring needed. On the NUCLEO-F446RE, USART2 (PA2 TX / PA3 RX) is routed
 * to the ST-LINK, which shows up on the PC as a virtual COM port.
 *
 * PC side, 115200 8N1, no flow control:
 *   macOS:  ls /dev/tty.usbmodem*   then   screen /dev/tty.usbmodemXXXX 115200
 *   or the terminal built into STM32CubeIDE (Window > Show View > Terminal).
 *
 * Expected output, one line per press:
 *   [42] Hello from STM32F446 over UART
 *
 * With the skeleton driver nothing arrives on the PC. That is the first thing
 * to fix: UART_PeriClockControl, UART_Init, UART_SetBaudRate,
 * UART_PeripheralControl and UART_SendData.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f446xx.h"

#define BUTTON_PIN      13   // B1 on PC13, active low

static UART_Handle_t uart2Handle;

static void delay(void) {
	for (volatile uint32_t i = 0; i < 500000; i++);
}

static void UART_GPIO_ConfigInit(void) {
	GPIO_Handle_t gpioUART;

	memset(&gpioUART, 0, sizeof(GPIO_Handle_t));

	gpioUART.Instance = GPIOA;
	gpioUART.Config.Mode = GPIO_MODE_ALTFN;
	gpioUART.Config.AltFunMode = 7;              // AF7 = USART1..3
	gpioUART.Config.OPType = GPIO_OP_TYPE_PP;
	gpioUART.Config.PuPdControl = GPIO_PIN_PU;   // UART line idles high
	gpioUART.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, ENABLE);

	// TX -> ST-LINK virtual COM port
	gpioUART.Config.PinNumber = 2;
	GPIO_Init(&gpioUART);

	// RX, not used here but configured so the line is not floating
	gpioUART.Config.PinNumber = 3;
	GPIO_Init(&gpioUART);
}

static void UART_ConfigInit(void) {
	memset(&uart2Handle, 0, sizeof(UART_Handle_t));

	uart2Handle.Instance = USART2;
	uart2Handle.Config.Mode = UART_MODE_ONLY_TX;
	uart2Handle.Config.Baud = UART_STD_BAUD_115200;    // BRR 0x8B at 16 MHz PCLK1
	uart2Handle.Config.NoOfStopBits = UART_STOPBITS_1;
	uart2Handle.Config.WordLength = UART_WORDLEN_8BITS;
	uart2Handle.Config.ParityControl = UART_PARITY_DISABLE;
	uart2Handle.Config.HWFlowControl = UART_HW_FLOW_CTRL_NONE;
	uart2Handle.Config.OverSampling = UART_OVERSAMPLING_16;

	UART_PeriClockControl(USART2, ENABLE);
	UART_Init(&uart2Handle);
}

static void GPIO_ButtonInit(void) {
	GPIO_Handle_t gpioButton;

	memset(&gpioButton, 0, sizeof(GPIO_Handle_t));

	gpioButton.Instance = GPIOC;
	gpioButton.Config.PinNumber = BUTTON_PIN;
	gpioButton.Config.Mode = GPIO_MODE_IN;
	gpioButton.Config.PuPdControl = GPIO_PIN_PU;
	gpioButton.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, ENABLE);
	GPIO_Init(&gpioButton);
}

int main(void) {
	char message[64];
	uint32_t count = 0;

	UART_GPIO_ConfigInit();
	UART_ConfigInit();
	GPIO_ButtonInit();

	UART_PeripheralControl(USART2, ENABLE);

	// Banner right after reset, so you can see the link works before pressing
	const char *banner = "\r\nuart_tx ready, press B1\r\n";
	UART_SendData(&uart2Handle, (uint8_t *)banner, strlen(banner));

	while (1) {
		// 1. Wait for button press (active low)
		while (GPIO_ReadFromInputPin(GPIOC, BUTTON_PIN) == 1);
		delay(); // Simple debounce delay

		// 2. Build and send the line. UART_SendData returns only after TC=1,
		//    so the buffer can be reused right after the call.
		int len = snprintf(message, sizeof(message),
		                   "[%lu] Hello from STM32F446 over UART\r\n", (unsigned long)count++);
		UART_SendData(&uart2Handle, (uint8_t *)message, (uint32_t)len);

		// 3. Wait for user to RELEASE the button before looping again
		while (GPIO_ReadFromInputPin(GPIOC, BUTTON_PIN) == 0);
		delay();
	}
}
