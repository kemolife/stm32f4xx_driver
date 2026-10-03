/*
 * test_uart.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  UART driver suite, USART1.
 *
 *  Pins, AF7: PA9 TX (D8), PA10 RX (D2).
 *
 *  [LOOP] jumper wire: PA9 (D8) <-> PA10 (D2).
 *
 *  UART_ReceiveData has no timeout, so it is only called after RXNE was seen
 *  with a raw register poll.
 *  Known good BRR values come from RM0390, not from the driver formula.
 */

#include <string.h>
#include "test_harness.h"

static UART_Handle_t uart1;

static volatile uint8_t tx_cmplt_count;
static volatile uint8_t rx_cmplt_count;
static volatile uint8_t error_count;
static volatile uint8_t last_error;

void USART1_IRQHandler(void) {
	UART_IRQHandling(&uart1);
}

void UART_ApplicationEventCallback(UART_Handle_t *pUARTHandle, uint8_t AppEv) {
	(void)pUARTHandle;

	if (AppEv == UART_EVENT_TX_CMPLT) {
		tx_cmplt_count++;
	} else if (AppEv == UART_EVENT_RX_CMPLT) {
		rx_cmplt_count++;
	} else if (AppEv >= UART_ERROR_PE) {
		error_count++;
		last_error = AppEv;
	}
}

static void reset_events(void) {
	tx_cmplt_count = 0;
	rx_cmplt_count = 0;
	error_count = 0;
	last_error = 0;
}

/* Drops any stale byte and clears ORE/FE/NF/PE/IDLE (read SR, then read DR) */
static void drain_rx_raw(UART_RegDef_t *pUARTx) {
	volatile uint32_t dummy;
	dummy = pUARTx->SR;
	dummy = pUARTx->DR;
	(void)dummy;
}

static int wait_rxne_raw(UART_RegDef_t *pUARTx) {
	return test_wait_reg(&pUARTx->SR, 1U << USART_SR_RXNE, 1, TEST_TIMEOUT_MS);
}

static void uart1_gpio_init(void) {
	GPIO_Handle_t pin;

	memset(&pin, 0, sizeof(pin));
	pin.Instance = GPIOA;
	pin.Config.Mode = GPIO_MODE_ALTFN;
	pin.Config.AltFunMode = 7;             // AF7 = USART1..3
	pin.Config.OPType = GPIO_OP_TYPE_PP;
	pin.Config.PuPdControl = GPIO_PIN_PU;  // idle line is high
	pin.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOA, ENABLE);

	pin.Config.PinNumber = 9;    // USART1_TX (D8)
	GPIO_Init(&pin);
	pin.Config.PinNumber = 10;   // USART1_RX (D2)
	GPIO_Init(&pin);
}

/* Clean start: reset, clock on, fresh handle, UART_Init. UE stays 0. */
static void uart1_setup(uint8_t wordlen, uint8_t parity, uint8_t stopbits,
                        uint8_t flow, uint8_t oversampling, uint32_t baud) {
	UART_PeriClockControl(USART1, ENABLE);
	UART_DeInit(USART1);

	memset(&uart1, 0, sizeof(uart1));
	uart1.Instance = USART1;
	uart1.Config.Mode = UART_MODE_TXRX;
	uart1.Config.Baud = baud;
	uart1.Config.NoOfStopBits = stopbits;
	uart1.Config.WordLength = wordlen;
	uart1.Config.ParityControl = parity;
	uart1.Config.HWFlowControl = flow;
	uart1.Config.OverSampling = oversampling;

	UART_Init(&uart1);
}

static void uart1_setup_8n1(void) {
	uart1_setup(UART_WORDLEN_8BITS, UART_PARITY_DISABLE, UART_STOPBITS_1,
	            UART_HW_FLOW_CTRL_NONE, UART_OVERSAMPLING_16, UART_STD_BAUD_115200);
}

/* ========================================================================== */
/*   No wiring                                                                */
/* ========================================================================== */

static void test_clock_control(void) {
	UART_PeriClockControl(USART1, ENABLE);
	CHECK(RCC->APB2ENR & (1U << 4), "USART1 clock not enabled (APB2ENR bit 4)");
	UART_PeriClockControl(USART1, DISABLE);
	CHECK(!(RCC->APB2ENR & (1U << 4)), "USART1 clock not disabled");

	UART_PeriClockControl(USART2, ENABLE);
	CHECK(RCC->APB1ENR & (1U << 17), "USART2 clock not enabled (APB1ENR bit 17)");
	UART_PeriClockControl(USART2, DISABLE);
	CHECK(!(RCC->APB1ENR & (1U << 17)), "USART2 clock not disabled");

	UART_PeriClockControl(UART4, ENABLE);
	CHECK(RCC->APB1ENR & (1U << 19), "UART4 clock not enabled (APB1ENR bit 19)");
	UART_PeriClockControl(UART4, DISABLE);

	UART_PeriClockControl(USART6, ENABLE);
	CHECK(RCC->APB2ENR & (1U << 5), "USART6 clock not enabled (APB2ENR bit 5)");
	UART_PeriClockControl(USART6, DISABLE);
}

static void test_deinit(void) {
	USART1_PCLK_EN();                   // raw macro: this test is about DeInit only
	USART1->CR1 = (1U << USART_CR1_TE) | (1U << USART_CR1_M);
	USART1->CR2 = (2U << USART_CR2_STOP);
	USART1->BRR = 0x1234U;

	UART_DeInit(USART1);

	CHECK(USART1->CR1 == 0U, "CR1 not back to reset value 0");
	CHECK(USART1->CR2 == 0U, "CR2 not back to reset value 0");
	CHECK(USART1->BRR == 0U, "BRR not back to reset value 0");
}

static void test_init_8n1_txrx(void) {
	uart1_setup_8n1();

	uint32_t cr1 = USART1->CR1;
	CHECK(cr1 & (1U << USART_CR1_TE),       "TE not set for UART_MODE_TXRX");
	CHECK(cr1 & (1U << USART_CR1_RE),       "RE not set for UART_MODE_TXRX");
	CHECK(!(cr1 & (1U << USART_CR1_M)),     "M set, expected 8 data bits");
	CHECK(!(cr1 & (1U << USART_CR1_PCE)),   "PCE set, expected no parity");
	CHECK(!(cr1 & (1U << USART_CR1_OVER8)), "OVER8 set, expected oversampling by 16");
	CHECK(!(cr1 & (1U << USART_CR1_UE)),    "UE set by UART_Init, it must be left to PeripheralControl");

	CHECK(((USART1->CR2 >> USART_CR2_STOP) & 0x3U) == UART_STOPBITS_1, "STOP bits not 1");
	CHECK(!(USART1->CR3 & (1U << USART_CR3_CTSE)), "CTSE set, expected no flow control");
	CHECK(!(USART1->CR3 & (1U << USART_CR3_RTSE)), "RTSE set, expected no flow control");
}

static void test_init_mode_tx_only_rx_only(void) {
	uart1_setup_8n1();
	UART_DeInit(USART1);
	uart1.Config.Mode = UART_MODE_ONLY_TX;
	UART_Init(&uart1);
	CHECK(USART1->CR1 & (1U << USART_CR1_TE),    "TE not set for UART_MODE_ONLY_TX");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_RE)), "RE set for UART_MODE_ONLY_TX");

	UART_DeInit(USART1);
	uart1.Config.Mode = UART_MODE_ONLY_RX;
	UART_Init(&uart1);
	CHECK(USART1->CR1 & (1U << USART_CR1_RE),    "RE not set for UART_MODE_ONLY_RX");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_TE)), "TE set for UART_MODE_ONLY_RX");
}

static void test_init_9o2_flow(void) {
	uart1_setup(UART_WORDLEN_9BITS, UART_PARITY_EN_ODD, UART_STOPBITS_2,
	            UART_HW_FLOW_CTRL_CTS_RTS, UART_OVERSAMPLING_8, UART_STD_BAUD_9600);

	uint32_t cr1 = USART1->CR1;
	CHECK(cr1 & (1U << USART_CR1_M),     "M not set for 9 data bits");
	CHECK(cr1 & (1U << USART_CR1_PCE),   "PCE not set for odd parity");
	CHECK(cr1 & (1U << USART_CR1_PS),    "PS not set for odd parity");
	CHECK(cr1 & (1U << USART_CR1_OVER8), "OVER8 not set for oversampling by 8");

	CHECK(((USART1->CR2 >> USART_CR2_STOP) & 0x3U) == UART_STOPBITS_2, "STOP bits not 2");
	CHECK(USART1->CR3 & (1U << USART_CR3_CTSE), "CTSE not set");
	CHECK(USART1->CR3 & (1U << USART_CR3_RTSE), "RTSE not set");

	/* Even parity must clear PS again */
	UART_DeInit(USART1);
	uart1.Config.ParityControl = UART_PARITY_EN_EVEN;
	UART_Init(&uart1);
	CHECK(USART1->CR1 & (1U << USART_CR1_PCE),   "PCE not set for even parity");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_PS)), "PS set for even parity");
}

static void test_baud_rate_golden_values(void) {
	/* RM0390 table "Error calculation for programmed baud rates", fPCLK = 16 MHz */
	if (RCC_GetPCLK2Value() != 16000000U) {
		SKIP("golden values need PCLK2 = 16 MHz");
		return;
	}

	uart1_setup(UART_WORDLEN_8BITS, UART_PARITY_DISABLE, UART_STOPBITS_1,
	            UART_HW_FLOW_CTRL_NONE, UART_OVERSAMPLING_16, UART_STD_BAUD_9600);
	CHECK(USART1->BRR == 0x0683U, "9600 @16MHz OVER16: BRR expected 0x0683 (104.1875)");

	uart1_setup_8n1();
	CHECK(USART1->BRR == 0x008BU, "115200 @16MHz OVER16: BRR expected 0x008B (8.6875)");

	uart1_setup(UART_WORDLEN_8BITS, UART_PARITY_DISABLE, UART_STOPBITS_1,
	            UART_HW_FLOW_CTRL_NONE, UART_OVERSAMPLING_8, UART_STD_BAUD_115200);
	CHECK(USART1->BRR == 0x0113U, "115200 @16MHz OVER8: BRR expected 0x0113 (17.375)");
	CHECK(!(USART1->BRR & (1U << 3)), "OVER8: BRR bit 3 must be 0");

	/* Direct call must give the same result as UART_Init */
	USART1->BRR = 0U;
	UART_SetBaudRate(USART1, UART_STD_BAUD_115200);
	CHECK(USART1->BRR == 0x0113U, "UART_SetBaudRate direct call gives wrong BRR");
}

static void test_peripheral_control(void) {
	uart1_setup_8n1();

	UART_PeripheralControl(USART1, ENABLE);
	CHECK(USART1->CR1 & (1U << USART_CR1_UE), "UE not set by PeripheralControl(ENABLE)");

	UART_PeripheralControl(USART1, DISABLE);
	CHECK(!(USART1->CR1 & (1U << USART_CR1_UE)), "UE not cleared by PeripheralControl(DISABLE)");
	CHECK(USART1->CR1 & (1U << USART_CR1_TE), "PeripheralControl changed other CR1 bits");
}

static void test_flag_status_and_clear(void) {
	uart1_setup_8n1();
	UART_PeripheralControl(USART1, ENABLE);

	/* After reset TXE = 1 and TC = 1 (SR reset value 0x00C0) */
	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_TXE) == UART_FLAG_SET,    "TXE should be SET when idle");
	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_RXNE) == UART_FLAG_RESET, "RXNE should be RESET");

	/* Wait for the idle frame that TE=1 sends, then clear TC */
	test_wait_reg(&USART1->SR, 1U << USART_SR_TC, 1, TEST_TIMEOUT_MS);
	UART_ClearFlag(USART1, UART_FLAG_TC);
	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_TC) == UART_FLAG_RESET, "TC not cleared by UART_ClearFlag");
	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_TXE) == UART_FLAG_SET,  "UART_ClearFlag(TC) touched TXE");

	UART_PeripheralControl(USART1, DISABLE);
}

static void test_nvic_config(void) {
	static const uint8_t irqs[] = { IRQ_NO_USART1, IRQ_NO_UART4, IRQ_NO_USART6 };

	for (uint32_t i = 0; i < sizeof(irqs); i++) {
		UART_IRQInterruptConfig(irqs[i], ENABLE);
		CHECK(test_nvic_is_enabled(irqs[i]), "UART IRQ not enabled in NVIC");

		UART_IRQPriorityConfig(irqs[i], NVIC_IRQ_PRI12);
		CHECK(test_nvic_priority(irqs[i]) == NVIC_IRQ_PRI12, "UART IRQ priority not 12");

		UART_IRQInterruptConfig(irqs[i], DISABLE);
		CHECK(!test_nvic_is_enabled(irqs[i]), "UART IRQ not disabled in NVIC");
	}
}

/* ========================================================================== */
/*   [LOOP] PA9 -> PA10                                                       */
/* ========================================================================== */

static int loop_wire_present(void) {
	int ok = test_wire_connected(GPIOA, 9, GPIOA, 10);
	uart1_gpio_init();   // the wire check left the pins as GPIO inputs

	if (!ok) {
		SKIP("jumper wire PA9 (D8) <-> PA10 (D2) missing");
	}
	return ok;
}

static void test_loop_polling_8n1(void) {
	static const uint8_t pattern[] = { 0x00, 0x55, 0xAA, 0xFF, 'H', 'e', 'l', 'l', 'o', '\n' };
	uint8_t rx = 0;

	if (!loop_wire_present()) {
		return;
	}

	uart1_setup_8n1();
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	for (uint32_t i = 0; i < sizeof(pattern); i++) {
		UART_SendData(&uart1, (uint8_t *)&pattern[i], 1);

		if (!wait_rxne_raw(USART1)) {
			CHECK(0, "no byte came back (UART_SendData sends nothing?)");
			break;
		}
		UART_ReceiveData(&uart1, &rx, 1);
		CHECK(rx == pattern[i], "received byte differs from sent byte");
	}

	/* SendData must return only after TC = 1 */
	CHECK(USART1->SR & (1U << USART_SR_TC), "TC not set after UART_SendData returned");

	UART_PeripheralControl(USART1, DISABLE);
}

static void test_loop_polling_9bit(void) {
	/* 9 data bits, no parity: each frame is a uint16_t, bit 8 must survive */
	static const uint16_t pattern[] = { 0x01FF, 0x0100, 0x00AA, 0x0155 };
	uint16_t rx = 0;

	if (!loop_wire_present()) {
		return;
	}

	uart1_setup(UART_WORDLEN_9BITS, UART_PARITY_DISABLE, UART_STOPBITS_1,
	            UART_HW_FLOW_CTRL_NONE, UART_OVERSAMPLING_16, UART_STD_BAUD_115200);
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	for (uint32_t i = 0; i < sizeof(pattern) / sizeof(pattern[0]); i++) {
		UART_SendData(&uart1, (uint8_t *)&pattern[i], 1);

		if (!wait_rxne_raw(USART1)) {
			CHECK(0, "no frame came back (9-bit)");
			break;
		}
		rx = 0;
		UART_ReceiveData(&uart1, (uint8_t *)&rx, 1);
		CHECK(rx == pattern[i], "9-bit frame differs, check the 0x01FF mask and uint16_t handling");
	}

	UART_PeripheralControl(USART1, DISABLE);
}

static void test_loop_polling_parity(void) {
	/* 8 bits + even parity = 7 data bits. Bytes < 0x80 must come back unchanged
	 * and PE must stay clear. */
	static const uint8_t pattern[] = { 'P', 'a', 'r', 0x7F, 0x01 };
	uint8_t rx = 0;

	if (!loop_wire_present()) {
		return;
	}

	uart1_setup(UART_WORDLEN_8BITS, UART_PARITY_EN_EVEN, UART_STOPBITS_1,
	            UART_HW_FLOW_CTRL_NONE, UART_OVERSAMPLING_16, UART_STD_BAUD_115200);
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	for (uint32_t i = 0; i < sizeof(pattern); i++) {
		UART_SendData(&uart1, (uint8_t *)&pattern[i], 1);

		if (!wait_rxne_raw(USART1)) {
			CHECK(0, "no byte came back (parity)");
			break;
		}
		CHECK(UART_GetFlagStatus(USART1, UART_FLAG_PE) == UART_FLAG_RESET, "parity error on loopback");
		UART_ReceiveData(&uart1, &rx, 1);
		CHECK(rx == pattern[i], "byte differs with parity, check the 0x7F mask");
	}

	UART_PeripheralControl(USART1, DISABLE);
}

static void test_loop_overrun_flag(void) {
	/* Send 3 bytes and never read DR: the 2nd byte finds RXNE still set -> ORE */
	uint8_t data[3] = { 1, 2, 3 };

	if (!loop_wire_present()) {
		return;
	}

	uart1_setup_8n1();
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	UART_SendData(&uart1, data, sizeof(data));
	test_delay_ms(1);   // let the last frame land

	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_ORE) == UART_FLAG_SET, "ORE not reported after overrun");

	drain_rx_raw(USART1);
	CHECK(UART_GetFlagStatus(USART1, UART_FLAG_ORE) == UART_FLAG_RESET, "ORE still set after read SR -> read DR");

	UART_PeripheralControl(USART1, DISABLE);
}

static void test_loop_interrupt_txrx(void) {
	static uint8_t tx[] = "Interrupt driven UART loopback 0123456789";
	static uint8_t rx[sizeof(tx)];

	if (!loop_wire_present()) {
		return;
	}

	memset(rx, 0, sizeof(rx));
	reset_events();

	uart1_setup_8n1();
	UART_IRQPriorityConfig(IRQ_NO_USART1, NVIC_IRQ_PRI12);
	UART_IRQInterruptConfig(IRQ_NO_USART1, ENABLE);
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	/* Arm RX first so the first looped-back byte is not missed */
	CHECK(UART_ReceiveDataIT(&uart1, rx, sizeof(tx)) == UART_READY, "ReceiveDataIT rejected on idle handle");
	CHECK(UART_SendDataIT(&uart1, tx, sizeof(tx)) == UART_READY,    "SendDataIT rejected on idle handle");

	CHECK(test_wait_count(&tx_cmplt_count, 1, TEST_TIMEOUT_MS), "no UART_EVENT_TX_CMPLT callback (timeout)");
	CHECK(test_wait_count(&rx_cmplt_count, 1, TEST_TIMEOUT_MS), "no UART_EVENT_RX_CMPLT callback (timeout)");

	CHECK(memcmp(tx, rx, sizeof(tx)) == 0, "IT received buffer differs from sent buffer");
	CHECK(tx_cmplt_count == 1, "TX_CMPLT fired more than once");
	CHECK(rx_cmplt_count == 1, "RX_CMPLT fired more than once");
	CHECK(error_count == 0,    "error callback during clean IT transfer");

	CHECK(uart1.TxBusyState == UART_READY, "TxBusyState not back to UART_READY");
	CHECK(uart1.RxBusyState == UART_READY, "RxBusyState not back to UART_READY");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_TXEIE)),  "TXEIE left on after TX complete");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_TCIE)),   "TCIE left on after TX complete");
	CHECK(!(USART1->CR1 & (1U << USART_CR1_RXNEIE)), "RXNEIE left on after RX complete");

	UART_IRQInterruptConfig(IRQ_NO_USART1, DISABLE);
	UART_PeripheralControl(USART1, DISABLE);
}

static void test_loop_interrupt_busy_rejection(void) {
	static uint8_t tx[32];
	static uint8_t rx[32];

	if (!loop_wire_present()) {
		return;
	}

	memset(tx, 'B', sizeof(tx));
	reset_events();

	uart1_setup_8n1();
	UART_IRQInterruptConfig(IRQ_NO_USART1, ENABLE);
	UART_PeripheralControl(USART1, ENABLE);
	drain_rx_raw(USART1);

	CHECK(UART_ReceiveDataIT(&uart1, rx, sizeof(rx)) == UART_READY,      "first ReceiveDataIT rejected");
	CHECK(UART_ReceiveDataIT(&uart1, rx, sizeof(rx)) == UART_BUSY_IN_RX, "second ReceiveDataIT not rejected");

	CHECK(UART_SendDataIT(&uart1, tx, sizeof(tx)) == UART_READY,      "first SendDataIT rejected");
	CHECK(UART_SendDataIT(&uart1, tx, sizeof(tx)) == UART_BUSY_IN_TX, "second SendDataIT not rejected");

	/* Let both transfers finish so the next test starts clean */
	CHECK(test_wait_count(&tx_cmplt_count, 1, TEST_TIMEOUT_MS), "TX never completed");
	CHECK(test_wait_count(&rx_cmplt_count, 1, TEST_TIMEOUT_MS), "RX never completed");

	/* After completion the handle must accept a new transfer */
	CHECK(UART_SendDataIT(&uart1, tx, 1) == UART_READY, "SendDataIT rejected after TX complete");
	CHECK(test_wait_count(&tx_cmplt_count, 2, TEST_TIMEOUT_MS), "second TX never completed");

	UART_IRQInterruptConfig(IRQ_NO_USART1, DISABLE);
	UART_PeripheralControl(USART1, DISABLE);
}

void test_suite_uart(void) {
	test_suite_begin("uart");

	test_run(test_clock_control,             "uart_clock_control");
	test_run(test_deinit,                    "uart_deinit");
	test_run(test_init_8n1_txrx,             "uart_init_8n1_txrx");
	test_run(test_init_mode_tx_only_rx_only, "uart_init_mode_tx_only_rx_only");
	test_run(test_init_9o2_flow,             "uart_init_9o2_flow");
	test_run(test_baud_rate_golden_values,   "uart_baud_rate_golden_values");
	test_run(test_peripheral_control,        "uart_peripheral_control");
	test_run(test_flag_status_and_clear,     "uart_flag_status_and_clear");
	test_run(test_nvic_config,               "uart_nvic_config");

	test_run(test_loop_polling_8n1,              "uart_loop_polling_8n1");
	test_run(test_loop_polling_9bit,             "uart_loop_polling_9bit");
	test_run(test_loop_polling_parity,           "uart_loop_polling_parity");
	test_run(test_loop_overrun_flag,             "uart_loop_overrun_flag");
	test_run(test_loop_interrupt_txrx,           "uart_loop_interrupt_txrx");
	test_run(test_loop_interrupt_busy_rejection, "uart_loop_interrupt_busy_rejection");

	UART_DeInit(USART1);
	UART_PeriClockControl(USART1, DISABLE);
}
