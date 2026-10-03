/*
 * uart_cmd_handling_it.c
 *
 *  Created on: 28 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * Interrupt driven command shell on USART2 (ST-LINK virtual COM port).
 * Type a command in a PC terminal, press Enter, the board answers.
 *
 * Commands:
 *   help      list commands
 *   led on    LD2 (PA5) on
 *   led off   LD2 off
 *   led       toggle LD2
 *   stats     received bytes and error counters
 *
 * PC side, 115200 8N1, no flow control:
 *   screen /dev/tty.usbmodemXXXX 115200     (quit: Ctrl-A then K)
 *
 * Data flow:
 *   RX  - UART_ReceiveDataIT, one byte at a time. The callback pushes the byte
 *         into a ring buffer and re-arms the next reception. No parsing in
 *         the ISR.
 *   main- pops bytes, echoes them (terminals do not echo locally), collects a
 *         line, runs the command on Enter.
 *   TX  - echo uses blocking UART_SendData (1 byte, short). The answer uses
 *         UART_SendDataIT and waits for UART_EVENT_TX_CMPLT.
 *
 * Driver parts this app needs: everything from uart_tx.c plus
 * UART_ReceiveDataIT, UART_SendDataIT, UART_IRQInterruptConfig,
 * UART_IRQPriorityConfig and UART_IRQHandling (RXNE, TXE, TC, ORE).
 *
 * Checks while typing:
 *   - Typing fast / pasting a long line must not raise "ore" in "stats".
 *     If it does, the ISR is too slow or RXNE is not re-armed fast enough.
 *   - "led" must react on every Enter, also after a paste.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f446xx.h"

#define LED_PIN         5     // LD2 on PA5
#define RX_RING_SIZE    64    // power of two, so the index wraps with a mask
#define LINE_MAX        32

static UART_Handle_t uart2Handle;

static uint8_t rx_byte;                           // target of each 1-byte ReceiveDataIT
static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head;                 // written by ISR only
static volatile uint32_t rx_tail;                 // written by main only

static volatile uint8_t tx_done = 1;
static volatile uint32_t rx_count;
static volatile uint32_t ore_count;
static volatile uint32_t err_count;
static volatile uint32_t drop_count;              // ring buffer full

static char tx_msg[128];

static void UART_GPIO_ConfigInit(void) {
	GPIO_Handle_t gpioUART;

	memset(&gpioUART, 0, sizeof(GPIO_Handle_t));

	gpioUART.Instance = GPIOA;
	gpioUART.Config.Mode = GPIO_MODE_ALTFN;
	gpioUART.Config.AltFunMode = 7;              // AF7 = USART1..3
	gpioUART.Config.OPType = GPIO_OP_TYPE_PP;
	gpioUART.Config.PuPdControl = GPIO_PIN_PU;
	gpioUART.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, DRV_ENABLE);

	gpioUART.Config.PinNumber = 2;   // USART2_TX
	GPIO_Init(&gpioUART);

	gpioUART.Config.PinNumber = 3;   // USART2_RX
	GPIO_Init(&gpioUART);
}

static void UART_ConfigInit(void) {
	memset(&uart2Handle, 0, sizeof(UART_Handle_t));

	uart2Handle.Instance = USART2;
	uart2Handle.Config.Mode = UART_MODE_TXRX;
	uart2Handle.Config.Baud = UART_STD_BAUD_115200;
	uart2Handle.Config.NoOfStopBits = UART_STOPBITS_1;
	uart2Handle.Config.WordLength = UART_WORDLEN_8BITS;
	uart2Handle.Config.ParityControl = UART_PARITY_DISABLE;
	uart2Handle.Config.HWFlowControl = UART_HW_FLOW_CTRL_NONE;
	uart2Handle.Config.OverSampling = UART_OVERSAMPLING_16;

	UART_PeriClockControl(USART2, DRV_ENABLE);
	UART_Init(&uart2Handle);

	UART_IRQPriorityConfig(IRQ_NO_USART2, NVIC_IRQ_PRI12);
	UART_IRQInterruptConfig(IRQ_NO_USART2, DRV_ENABLE);
}

static void GPIO_LedInit(void) {
	GPIO_Handle_t gpioLed;

	memset(&gpioLed, 0, sizeof(GPIO_Handle_t));

	gpioLed.Instance = GPIOA;
	gpioLed.Config.PinNumber = LED_PIN;
	gpioLed.Config.Mode = GPIO_MODE_OUT;
	gpioLed.Config.OPType = GPIO_OP_TYPE_PP;
	gpioLed.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioLed.Config.Speed = GPIO_SPEED_LOW;

	GPIO_PeriClockControl(GPIOA, DRV_ENABLE);
	GPIO_Init(&gpioLed);
}

void USART2_IRQHandler(void) {
	UART_IRQHandling(&uart2Handle);
}

void UART_ApplicationEventCallback(UART_Handle_t *pUARTHandle, uint8_t AppEv) {
	if (AppEv == UART_EVENT_RX_CMPLT) {
		uint32_t next = (rx_head + 1U) & (RX_RING_SIZE - 1U);

		if (next != rx_tail) {
			rx_ring[rx_head] = rx_byte;
			rx_head = next;
		} else {
			drop_count++;
		}
		rx_count++;

		// Re-arm for the next byte. The driver set RxBusyState back to
		// UART_READY before calling us, so this is accepted.
		UART_ReceiveDataIT(pUARTHandle, &rx_byte, 1);
	} else if (AppEv == UART_EVENT_TX_CMPLT) {
		tx_done = 1;
	} else if (AppEv == UART_ERROR_ORE) {
		ore_count++;
	} else if (AppEv >= UART_ERROR_PE) {
		err_count++;
	}
}

static int rx_pop(uint8_t *out) {
	if (rx_tail == rx_head) {
		return 0;
	}
	*out = rx_ring[rx_tail];
	rx_tail = (rx_tail + 1U) & (RX_RING_SIZE - 1U);
	return 1;
}

static void send_it(const char *text) {
	// The IT buffer must stay valid until TX_CMPLT, so it is a static, and we
	// wait for the previous answer before overwriting it.
	while (!tx_done);
	tx_done = 0;

	strncpy(tx_msg, text, sizeof(tx_msg) - 1U);
	tx_msg[sizeof(tx_msg) - 1U] = '\0';

	if (UART_SendDataIT(&uart2Handle, (uint8_t *)tx_msg, strlen(tx_msg)) != UART_READY) {
		tx_done = 1;   // rejected, do not block the next answer forever
	}
}

static void run_command(const char *line) {
	char answer[96];

	if (strcmp(line, "help") == 0) {
		send_it("commands: help, led, led on, led off, stats\r\n");
	} else if (strcmp(line, "led on") == 0) {
		GPIO_WriteToOutputPin(GPIOA, LED_PIN, GPIO_PIN_SET);
		send_it("LED on\r\n");
	} else if (strcmp(line, "led off") == 0) {
		GPIO_WriteToOutputPin(GPIOA, LED_PIN, GPIO_PIN_RESET);
		send_it("LED off\r\n");
	} else if (strcmp(line, "led") == 0) {
		GPIO_ToggleOutputPin(GPIOA, LED_PIN);
		send_it("LED toggled\r\n");
	} else if (strcmp(line, "stats") == 0) {
		snprintf(answer, sizeof(answer), "rx=%lu ore=%lu err=%lu drop=%lu\r\n",
		         (unsigned long)rx_count, (unsigned long)ore_count,
		         (unsigned long)err_count, (unsigned long)drop_count);
		send_it(answer);
	} else if (line[0] != '\0') {
		snprintf(answer, sizeof(answer), "unknown command \"%s\", type help\r\n", line);
		send_it(answer);
	}
}

int main(void) {
	char line[LINE_MAX + 1];
	uint32_t line_len = 0;
	uint8_t ch;

	GPIO_LedInit();
	UART_GPIO_ConfigInit();
	UART_ConfigInit();

	UART_PeripheralControl(USART2, DRV_ENABLE);

	send_it("\r\nuart_cmd_handling_it ready, type help\r\n> ");

	// Arm the first reception; the callback keeps it going from here
	UART_ReceiveDataIT(&uart2Handle, &rx_byte, 1);

	while (1) {
		if (!rx_pop(&ch)) {
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			// Wait for any pending IT answer, then echo the newline blocking
			while (!tx_done);
			UART_SendData(&uart2Handle, (uint8_t *)"\r\n", 2);

			line[line_len] = '\0';
			run_command(line);
			line_len = 0;

			send_it("> ");
		} else if ((ch == 0x7F || ch == '\b') && line_len > 0U) {
			// Backspace: erase the char on the terminal too
			line_len--;
			while (!tx_done);
			UART_SendData(&uart2Handle, (uint8_t *)"\b \b", 3);
		} else if (ch >= ' ' && line_len < LINE_MAX) {
			line[line_len++] = (char)ch;
			while (!tx_done);
			UART_SendData(&uart2Handle, &ch, 1);   // echo
		}
	}
}
