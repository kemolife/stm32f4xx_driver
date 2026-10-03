/*
 * test_spi.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  SPI driver suite, SPI2 as master with software NSS (SSM=1, SSI=1).
 *
 *  Pins, AF5: PB13 SCK, PB14 MISO, PB15 MOSI. NSS is not used.
 *
 *  [LOOP] jumper: PB15 MOSI (CN10 pin 26) <-> PB14 MISO (CN10 pin 28).
 *  The two pins are side by side, one jumper cap fits.
 *  A master always receives a byte for every byte it clocks out, so the
 *  blocking API cannot hang here even without the wire. Without the wire the
 *  data would be wrong, which is why the wire is checked first.
 */

#include <string.h>
#include "test_harness.h"

static SPI_Handle_t spi2;

static volatile uint8_t spi_tx_cmplt;
static volatile uint8_t spi_rx_cmplt;
static volatile uint8_t spi_ovr;

void SPI2_IRQHandler(void) {
	SPI_IRQHandling(&spi2);
}

void SPI_ApplicationEventCallback(SPI_Handle_t *pSPIHandle, uint8_t AppEv) {
	(void)pSPIHandle;

	if (AppEv == SPI_EVENT_TX_CMPLT) {
		spi_tx_cmplt++;
	} else if (AppEv == SPI_EVENT_RX_CMPLT) {
		spi_rx_cmplt++;
	} else if (AppEv == SPI_EVENT_OVR_ERR) {
		spi_ovr++;
	}
}

static void reset_events(void) {
	spi_tx_cmplt = 0;
	spi_rx_cmplt = 0;
	spi_ovr = 0;
}

static void spi2_gpio_init(void) {
	GPIO_Handle_t pin;

	memset(&pin, 0, sizeof(pin));
	pin.Instance = GPIOB;
	pin.Config.Mode = GPIO_MODE_ALTFN;
	pin.Config.AltFunMode = 5;
	pin.Config.OPType = GPIO_OP_TYPE_PP;
	pin.Config.PuPdControl = GPIO_NOT_PUPD;
	pin.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOB, ENABLE);

	pin.Config.PinNumber = 13;   // SCK
	GPIO_Init(&pin);
	pin.Config.PinNumber = 14;   // MISO
	GPIO_Init(&pin);
	pin.Config.PinNumber = 15;   // MOSI
	GPIO_Init(&pin);
}

/* Clean start: reset, clock on, fresh handle, SPI_Init. SPE stays 0. */
static void spi2_setup(uint8_t bus, uint8_t dff, uint8_t cpol, uint8_t cpha, uint8_t ssm) {
	SPI_PeriClockControl(SPI2, ENABLE);
	SPI_DeInit(SPI2);

	memset(&spi2, 0, sizeof(spi2));
	spi2.Instance = SPI2;
	spi2.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
	spi2.Config.BusConfig = bus;
	spi2.Config.SclkSpeed = SPI_SCLK_SPEED_DIV32;   // 500 kHz at 16 MHz PCLK1
	spi2.Config.DFF = dff;
	spi2.Config.CPOL = cpol;
	spi2.Config.CPHA = cpha;
	spi2.Config.SSM = ssm;

	SPI_Init(&spi2);
}

static void spi2_setup_default(void) {
	spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_8BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_EN);
}

/* Drops a stale byte and clears OVR (read DR, then read SR) */
static void drain_rx_raw(void) {
	volatile uint32_t dummy;
	dummy = SPI2->DR;
	dummy = SPI2->SR;
	(void)dummy;
}

/* ========================================================================== */
/*   No wiring                                                                */
/* ========================================================================== */

static void test_clock_control(void) {
	SPI_PeriClockControl(SPI1, ENABLE);
	CHECK(RCC->APB2ENR & (1U << 12), "SPI1 clock not enabled (APB2ENR bit 12)");
	SPI_PeriClockControl(SPI1, DISABLE);
	CHECK(!(RCC->APB2ENR & (1U << 12)), "SPI1 clock not disabled");

	SPI_PeriClockControl(SPI2, ENABLE);
	CHECK(RCC->APB1ENR & (1U << 14), "SPI2 clock not enabled (APB1ENR bit 14)");

	SPI_PeriClockControl(SPI3, ENABLE);
	CHECK(RCC->APB1ENR & (1U << 15), "SPI3 clock not enabled (APB1ENR bit 15)");
	SPI_PeriClockControl(SPI3, DISABLE);
	CHECK(!(RCC->APB1ENR & (1U << 15)), "SPI3 clock not disabled");

	SPI_PeriClockControl(SPI4, ENABLE);
	CHECK(RCC->APB2ENR & (1U << 13), "SPI4 clock not enabled (APB2ENR bit 13)");
	SPI_PeriClockControl(SPI4, DISABLE);
}

static void test_deinit(void) {
	SPI2_PCLK_EN();                     // raw macro: this test is about DeInit only
	SPI2->CR1 = (1U << SPI_CR1_MSTR) | (1U << SPI_CR1_DFF);
	SPI2->CR2 = (1U << SPI_CR2_SSOE);

	SPI_DeInit(SPI2);

	CHECK(SPI2->CR1 == 0U, "CR1 not back to reset value 0");
	CHECK(SPI2->CR2 == 0U, "CR2 not back to reset value 0");
}

static void test_init_master_fields(void) {
	spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_8BITS, SPI_CPOL_HIGH, SPI_CPHA_HIGH, SPI_SSM_EN);

	uint32_t cr1 = SPI2->CR1;
	CHECK(cr1 & (1U << SPI_CR1_MSTR),        "MSTR not set");
	CHECK(((cr1 >> SPI_CR1_BR) & 0x7U) == SPI_SCLK_SPEED_DIV32, "BR not DIV32 (100)");
	CHECK(cr1 & (1U << SPI_CR1_CPOL),        "CPOL not set");
	CHECK(cr1 & (1U << SPI_CR1_CPHA),        "CPHA not set");
	CHECK(cr1 & (1U << SPI_CR1_SSM),         "SSM not set");
	CHECK(cr1 & (1U << SPI_CR1_SSI),         "SSI not set: master with SSM needs SSI=1 or MODF");
	CHECK(!(cr1 & (1U << SPI_CR1_DFF)),      "DFF set, expected 8-bit");
	CHECK(!(cr1 & (1U << SPI_CR1_BIDIMODE)), "BIDIMODE set for full duplex");
	CHECK(!(cr1 & (1U << SPI_CR1_RXONLY)),   "RXONLY set for full duplex");
	CHECK(!(cr1 & (1U << SPI_CR1_SPE)),      "SPE set by SPI_Init, it must be left to PeripheralControl");
}

static void test_init_bus_configs(void) {
	spi2_setup(SPI_BUS_CONFIG_HALF_DUPLEX, SPI_DFF_8BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_EN);
	CHECK(SPI2->CR1 & (1U << SPI_CR1_BIDIMODE), "half duplex: BIDIMODE not set");

	spi2_setup(SPI_BUS_CONFIG_SIMPLEX_RXONLY, SPI_DFF_8BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_EN);
	CHECK(!(SPI2->CR1 & (1U << SPI_CR1_BIDIMODE)), "simplex rx: BIDIMODE set");
	CHECK(SPI2->CR1 & (1U << SPI_CR1_RXONLY),      "simplex rx: RXONLY not set");

	spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_16BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_EN);
	CHECK(SPI2->CR1 & (1U << SPI_CR1_DFF), "DFF not set for 16-bit");
	CHECK(!(SPI2->CR1 & ((1U << SPI_CR1_CPOL) | (1U << SPI_CR1_CPHA))), "CPOL/CPHA set for mode 0");
}

static void test_init_hardware_nss(void) {
	spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_8BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_DI);
	CHECK(!(SPI2->CR1 & (1U << SPI_CR1_SSM)), "SSM set for hardware NSS");
	CHECK(SPI2->CR2 & (1U << SPI_CR2_SSOE),   "SSOE not set: hardware NSS master needs it or MODF");
}

static void test_peripheral_control(void) {
	spi2_setup_default();

	SPI_PeripheralControl(SPI2, ENABLE);
	CHECK(SPI2->CR1 & (1U << SPI_CR1_SPE), "SPE not set by PeripheralControl(ENABLE)");

	/* Real hardware check: a bad NSS setup raises MODF, which clears MSTR */
	CHECK(!(SPI2->SR & (1U << SPI_SR_MODF)), "MODF raised after enable (NSS setup wrong)");
	CHECK(SPI2->CR1 & (1U << SPI_CR1_MSTR),  "MSTR cleared by hardware (mode fault)");
	CHECK(SPI_IsBusy(SPI2) == 0,             "SPI_IsBusy reports busy on an idle bus");

	SPI_PeripheralControl(SPI2, DISABLE);
	CHECK(!(SPI2->CR1 & (1U << SPI_CR1_SPE)), "SPE not cleared by PeripheralControl(DISABLE)");
}

static void test_nvic_config(void) {
	/* SPI2 = IRQ 36 -> ISER1 bit 4 */
	SPI_IRQInterruptConfig(IRQ_NO_SPI2, ENABLE);
	CHECK(test_nvic_is_enabled(IRQ_NO_SPI2), "SPI2 IRQ not enabled in NVIC");

	SPI_IRQPriorityConfig(IRQ_NO_SPI2, NVIC_IRQ_PRI11);
	CHECK(test_nvic_priority(IRQ_NO_SPI2) == NVIC_IRQ_PRI11, "SPI2 (IRQ 36) priority not 11");

	SPI_IRQPriorityConfig(IRQ_NO_SPI4, NVIC_IRQ_PRI9);
	CHECK(test_nvic_priority(IRQ_NO_SPI4) == NVIC_IRQ_PRI9, "SPI4 (IRQ 84) priority not 9");

	SPI_IRQInterruptConfig(IRQ_NO_SPI2, DISABLE);
	CHECK(!test_nvic_is_enabled(IRQ_NO_SPI2), "SPI2 IRQ not disabled in NVIC");
}

/* ========================================================================== */
/*   [LOOP] PB15 MOSI -> PB14 MISO                                            */
/* ========================================================================== */

static int loop_wire_present(void) {
	int ok = test_wire_connected(GPIOB, 15, GPIOB, 14);
	spi2_gpio_init();   // the wire check left the pins as GPIO inputs

	if (!ok) {
		SKIP("jumper PB15 <-> PB14 (CN10 pin 26-28) missing");
	}
	return ok;
}

static void test_loop_polling_8bit(void) {
	static const uint8_t pattern[] = { 0x00, 0xFF, 0x55, 0xAA, 0x01, 0x80, 'S', 'P', 'I' };
	uint8_t rx;

	if (!loop_wire_present()) {
		return;
	}

	spi2_setup_default();
	SPI_PeripheralControl(SPI2, ENABLE);
	drain_rx_raw();

	for (uint32_t i = 0; i < sizeof(pattern); i++) {
		rx = 0;
		SPI_SendData(SPI2, (uint8_t *)&pattern[i], 1);
		SPI_ReceiveData(SPI2, &rx, 1);
		CHECK(rx == pattern[i], "8-bit loopback byte differs");
	}

	CHECK(test_wait_reg(&SPI2->SR, 1U << SPI_SR_BSY, 0, TEST_TIMEOUT_MS), "BSY did not clear");
	CHECK(!(SPI2->SR & (1U << SPI_SR_OVR)), "OVR set during 1-by-1 transfer");

	SPI_PeripheralControl(SPI2, DISABLE);
}

static void test_loop_polling_16bit(void) {
	static const uint16_t pattern[] = { 0xA55A, 0x0001, 0x8000, 0xFFFF, 0x1234 };
	uint16_t rx;

	if (!loop_wire_present()) {
		return;
	}

	/* DFF must be written while SPE = 0, so the setup comes before enable */
	spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_16BITS, SPI_CPOL_LOW, SPI_CPHA_LOW, SPI_SSM_EN);
	SPI_PeripheralControl(SPI2, ENABLE);
	drain_rx_raw();

	for (uint32_t i = 0; i < sizeof(pattern) / sizeof(pattern[0]); i++) {
		rx = 0;
		SPI_SendData(SPI2, (uint8_t *)&pattern[i], 2);
		SPI_ReceiveData(SPI2, (uint8_t *)&rx, 2);
		CHECK(rx == pattern[i], "16-bit loopback frame differs");
	}

	/* Odd length in 16-bit mode must be refused, not half sent */
	uint8_t odd[3] = { 1, 2, 3 };
	SPI_SendData(SPI2, odd, 3);
	test_delay_ms(1);
	CHECK(!(SPI2->SR & (1U << SPI_SR_RXNE)), "odd length in 16-bit mode was sent");

	SPI_PeripheralControl(SPI2, DISABLE);
}

static void test_loop_all_modes(void) {
	if (!loop_wire_present()) {
		return;
	}

	for (uint8_t mode = 0; mode < 4U; mode++) {
		uint8_t tx = (uint8_t)(0x3C ^ mode), rx = 0;

		spi2_setup(SPI_BUS_CONFIG_FULL_DUPLEX, SPI_DFF_8BITS, (mode >> 1) & 1U, mode & 1U, SPI_SSM_EN);
		SPI_PeripheralControl(SPI2, ENABLE);
		drain_rx_raw();

		SPI_SendData(SPI2, &tx, 1);
		SPI_ReceiveData(SPI2, &rx, 1);

		if (rx != tx) {
			printf("    SPI mode %u: sent 0x%02X got 0x%02X\n", mode, tx, rx);
		}
		CHECK(rx == tx, "loopback failed in one of the CPOL/CPHA modes");

		SPI_PeripheralControl(SPI2, DISABLE);
	}
}

static void test_loop_interrupt_txrx(void) {
	static uint8_t tx[] = "SPI interrupt loopback 0123456789";
	static uint8_t rx[sizeof(tx)];

	if (!loop_wire_present()) {
		return;
	}

	memset(rx, 0, sizeof(rx));
	reset_events();

	spi2_setup_default();
	SPI_IRQPriorityConfig(IRQ_NO_SPI2, NVIC_IRQ_PRI11);
	SPI_IRQInterruptConfig(IRQ_NO_SPI2, ENABLE);
	SPI_PeripheralControl(SPI2, ENABLE);
	drain_rx_raw();

	/* Arm RX first: every TX byte produces one RX byte right away */
	CHECK(SPI_ReceiveDataIT(&spi2, rx, sizeof(tx)) == SPI_READY, "ReceiveDataIT rejected on idle handle");
	CHECK(SPI_SendDataIT(&spi2, tx, sizeof(tx)) == SPI_READY,    "SendDataIT rejected on idle handle");

	CHECK(test_wait_count(&spi_tx_cmplt, 1, TEST_TIMEOUT_MS), "no SPI_EVENT_TX_CMPLT (timeout)");
	CHECK(test_wait_count(&spi_rx_cmplt, 1, TEST_TIMEOUT_MS), "no SPI_EVENT_RX_CMPLT (timeout)");

	CHECK(memcmp(tx, rx, sizeof(tx)) == 0, "IT received buffer differs from sent buffer");
	CHECK(spi_ovr == 0, "overrun during IT transfer");
	CHECK(spi2.TxState == SPI_READY, "TxState not back to SPI_READY");
	CHECK(spi2.RxState == SPI_READY, "RxState not back to SPI_READY");
	CHECK(!(SPI2->CR2 & (1U << SPI_CR2_TXEIE)),  "TXEIE left on");
	CHECK(!(SPI2->CR2 & (1U << SPI_CR2_RXNEIE)), "RXNEIE left on");

	SPI_IRQInterruptConfig(IRQ_NO_SPI2, DISABLE);
	SPI_PeripheralControl(SPI2, DISABLE);
}

static void test_loop_busy_rejection(void) {
	static uint8_t tx[16];

	if (!loop_wire_present()) {
		return;
	}

	memset(tx, 0xA5, sizeof(tx));
	reset_events();

	spi2_setup_default();
	SPI_IRQInterruptConfig(IRQ_NO_SPI2, ENABLE);
	SPI_PeripheralControl(SPI2, ENABLE);

	CHECK(SPI_SendDataIT(&spi2, tx, sizeof(tx)) == SPI_READY,      "first SendDataIT rejected");
	CHECK(SPI_SendDataIT(&spi2, tx, sizeof(tx)) == SPI_BUSY_IN_TX, "second SendDataIT not rejected");
	CHECK(test_wait_count(&spi_tx_cmplt, 1, TEST_TIMEOUT_MS), "TX never completed");

	CHECK(SPI_SendDataIT(&spi2, tx, 1) == SPI_READY, "SendDataIT rejected after TX complete");
	CHECK(test_wait_count(&spi_tx_cmplt, 2, TEST_TIMEOUT_MS), "second TX never completed");

	SPI_IRQInterruptConfig(IRQ_NO_SPI2, DISABLE);
	test_wait_reg(&SPI2->SR, 1U << SPI_SR_BSY, 0, TEST_TIMEOUT_MS);
	drain_rx_raw();   // RX was not read, clear the OVR it caused
	SPI_PeripheralControl(SPI2, DISABLE);
}

static void test_loop_overrun_irq(void) {
	uint8_t data[3] = { 1, 2, 3 };

	if (!loop_wire_present()) {
		return;
	}

	reset_events();

	spi2_setup_default();
	SPI2->CR2 |= (1U << SPI_CR2_ERRIE);   // no driver API for ERRIE, same as the apps
	SPI_IRQInterruptConfig(IRQ_NO_SPI2, ENABLE);
	SPI_PeripheralControl(SPI2, ENABLE);
	drain_rx_raw();

	/* 3 bytes out, none read: the 2nd one finds RXNE still set -> OVR */
	SPI_SendData(SPI2, data, sizeof(data));
	test_wait_reg(&SPI2->SR, 1U << SPI_SR_BSY, 0, TEST_TIMEOUT_MS);

	CHECK(test_wait_count(&spi_ovr, 1, TEST_TIMEOUT_MS), "no SPI_EVENT_OVR_ERR callback");
	CHECK(!(SPI2->SR & (1U << SPI_SR_OVR)), "OVR not cleared by the driver (read DR, then SR)");

	SPI_IRQInterruptConfig(IRQ_NO_SPI2, DISABLE);
	SPI_PeripheralControl(SPI2, DISABLE);
}

void test_suite_spi(void) {
	test_suite_begin("spi");

	test_run(test_clock_control,       "spi_clock_control");
	test_run(test_deinit,              "spi_deinit");
	test_run(test_init_master_fields,  "spi_init_master_fields");
	test_run(test_init_bus_configs,    "spi_init_bus_configs");
	test_run(test_init_hardware_nss,   "spi_init_hardware_nss");
	test_run(test_peripheral_control,  "spi_peripheral_control");
	test_run(test_nvic_config,         "spi_nvic_config");

	test_run(test_loop_polling_8bit,   "spi_loop_polling_8bit");
	test_run(test_loop_polling_16bit,  "spi_loop_polling_16bit");
	test_run(test_loop_all_modes,      "spi_loop_all_modes");
	test_run(test_loop_interrupt_txrx, "spi_loop_interrupt_txrx");
	test_run(test_loop_busy_rejection, "spi_loop_busy_rejection");
	test_run(test_loop_overrun_irq,    "spi_loop_overrun_irq");

	SPI_DeInit(SPI2);
	SPI_PeriClockControl(SPI2, DISABLE);
}
