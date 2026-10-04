/*
 * test_i2c.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  I2C driver suite. I2C1 is the master, I2C3 is the slave, on the same board.
 *
 *  Pins, AF4, open-drain:
 *    I2C1 master: PB8 SCL (D15), PB9 SDA (D14)
 *    I2C3 slave:  PA8 SCL (D7),  PC9 SDA (CN10 pin 1)
 *  (I2C2 is not used: on this package its SDA is PB3, which is SWO.)
 *
 *  [LOOP] wiring, 2 wires + 2 resistors:
 *    PB8 (D15) <-> PA8 (D7)          SCL
 *    PB9 (D14) <-> PC9 (CN10 pin 1)  SDA
 *    4.7k from SCL to 3V3 and 4.7k from SDA to 3V3.
 *  Internal pull-ups are also enabled, which is often enough at 100 kHz with
 *  short wires, but external ones are the correct setup.
 *
 *  The blocking master API stops on a NACK (DRV_ERROR) and on its Timeout
 *  (DRV_TIMEOUT), so no test here can hang the board.
 */

#include <string.h>
#include "test_harness.h"

#define MASTER_OWN_ADDR   0x61
#define SLAVE_ADDR        0x68
#define ABSENT_ADDR       0x55

static I2C_Handle_t i2c1;   /* master */
static I2C_Handle_t i2c3;   /* slave  */

/* Master side events */
static volatile uint8_t m_tx_cmplt;
static volatile uint8_t m_rx_cmplt;
static volatile uint8_t m_af;
static volatile uint8_t m_err;

/* Slave side state */
static volatile uint8_t s_rx_buf[16];
static volatile uint8_t s_rx_len;
static volatile uint8_t s_tx_buf[16];
static volatile uint8_t s_tx_idx;
static volatile uint8_t s_stop;

void I2C1_EV_IRQHandler(void) { I2C_EV_IRQHandling(&i2c1); }
void I2C1_ER_IRQHandler(void) { I2C_ER_IRQHandling(&i2c1); }
void I2C3_EV_IRQHandler(void) { I2C_EV_IRQHandling(&i2c3); }
void I2C3_ER_IRQHandler(void) { I2C_ER_IRQHandling(&i2c3); }

void I2C_ApplicationEventCallback(I2C_Handle_t *pI2CHandle, uint8_t AppEv) {
	if (pI2CHandle == &i2c3) {
		/* Slave: behaves like a tiny register device */
		if (AppEv == I2C_EVENT_DATA_RCV) {
			uint8_t b = I2C_SlaveReceiveData(I2C3);
			if (s_rx_len < sizeof(s_rx_buf)) {
				s_rx_buf[s_rx_len++] = b;
			}
		} else if (AppEv == I2C_EVENT_DATA_REQ) {
			uint8_t b = (s_tx_idx < sizeof(s_tx_buf)) ? s_tx_buf[s_tx_idx] : 0xFF;
			s_tx_idx++;
			I2C_SlaveSendData(I2C3, b);
		} else if (AppEv == I2C_EVENT_STOP) {
			s_stop++;
		}
		/* I2C_ERROR_AF on the slave is normal: the master NACKs the last byte it reads */
		return;
	}

	if (AppEv == I2C_EVENT_TX_CMPLT) {
		m_tx_cmplt++;
	} else if (AppEv == I2C_EVENT_RX_CMPLT) {
		m_rx_cmplt++;
	} else if (AppEv == I2C_ERROR_AF) {
		m_af++;
	} else if (AppEv >= I2C_ERROR_BERR) {
		m_err++;
	}
}

static void reset_events(void) {
	m_tx_cmplt = 0;
	m_rx_cmplt = 0;
	m_af = 0;
	m_err = 0;
	memset((void *)s_rx_buf, 0, sizeof(s_rx_buf));
	s_rx_len = 0;
	s_tx_idx = 0;
	s_stop = 0;
}

static void i2c_pin(GPIO_RegDef_t *port, uint8_t pin) {
	GPIO_Handle_t h;

	memset(&h, 0, sizeof(h));
	h.Instance = port;
	h.Config.PinNumber = pin;
	h.Config.Mode = GPIO_MODE_ALTFN;
	h.Config.AltFunMode = 4;
	h.Config.OPType = GPIO_OP_TYPE_OD;
	h.Config.PuPdControl = GPIO_PIN_PU;
	h.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(port, DRV_ENABLE);
	GPIO_Init(&h);
}

static void i2c_gpio_init(void) {
	i2c_pin(GPIOB, 8);   // I2C1 SCL
	i2c_pin(GPIOB, 9);   // I2C1 SDA
	i2c_pin(GPIOA, 8);   // I2C3 SCL
	i2c_pin(GPIOC, 9);   // I2C3 SDA
}

/* Clean start for one peripheral: reset, clock on, fresh handle, I2C_Init. PE stays 0. */
static void i2c_setup(I2C_Handle_t *h, I2C_RegDef_t *regs, uint32_t speed,
                      uint8_t duty, uint8_t own_addr) {
	I2C_PeriClockControl(regs, DRV_ENABLE);
	I2C_DeInit(regs);

	memset(h, 0, sizeof(*h));
	h->Instance = regs;
	h->Config.SCLSpeed = speed;
	h->Config.DeviceAddress = own_addr;
	h->Config.ACKControl = I2C_ACK_ENABLE;
	h->Config.FMDutyCycle = duty;

	I2C_Init(h);
}

/* ========================================================================== */
/*   No wiring                                                                */
/* ========================================================================== */

static void test_clock_control(void) {
	I2C_PeriClockControl(I2C1, DRV_ENABLE);
	CHECK(RCC->APB1ENR & (1U << 21), "I2C1 clock not enabled (APB1ENR bit 21)");
	I2C_PeriClockControl(I2C2, DRV_ENABLE);
	CHECK(RCC->APB1ENR & (1U << 22), "I2C2 clock not enabled (APB1ENR bit 22)");
	I2C_PeriClockControl(I2C3, DRV_ENABLE);
	CHECK(RCC->APB1ENR & (1U << 23), "I2C3 clock not enabled (APB1ENR bit 23)");

	I2C_PeriClockControl(I2C2, DRV_DISABLE);
	CHECK(!(RCC->APB1ENR & (1U << 22)), "I2C2 clock not disabled");
}

static int pclk1_is_16mhz(void) {
	if (RCC_GetPCLK1Value() != 16000000U) {
		SKIP("golden values need PCLK1 = 16 MHz");
		return 0;
	}
	return 1;
}

static void test_init_standard_100k(void) {
	if (!pclk1_is_16mhz()) {
		return;
	}

	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_SM, I2C_FM_DUTY_2, SLAVE_ADDR);

	/* RM0390 27.6: FREQ = PCLK1 in MHz. SM: CCR = PCLK1 / (2 * 100k) = 80.
	 * TRISE = 1000 ns / 62.5 ns + 1 = 17 */
	CHECK((I2C1->CR2 & 0x3FU) == 16U, "CR2 FREQ not 16");
	CHECK(I2C1->CCR == 80U,           "SM 100k: CCR expected 0x0050 (80), FS=0");
	CHECK(I2C1->TRISE == 17U,         "SM: TRISE expected 17");
	CHECK(I2C1->OAR1 == (((uint32_t)SLAVE_ADDR << 1) | (1U << 14)),
	      "OAR1 expected (addr << 1) | bit 14, 7-bit mode");
	CHECK(!(I2C1->CR1 & (1U << I2C_CR1_PE)), "PE set by I2C_Init, it must be left to PeripheralControl");
}

static void test_init_fast_400k(void) {
	if (!pclk1_is_16mhz()) {
		return;
	}

	/* DUTY=0: Tlow/Thigh = 2, CCR = 16 MHz / (3 * 400k) = 13.
	 * FM TRISE = 300 ns / 62.5 ns + 1 = 4 + 1 = 5 */
	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_FM4K, I2C_FM_DUTY_2, SLAVE_ADDR);
	CHECK(I2C1->CCR == ((1U << I2C_CCR_FS) | 13U), "FM 400k duty 2: CCR expected 0x800D");
	CHECK(I2C1->TRISE == 5U,                       "FM: TRISE expected 5");

	/* DUTY=1: Tlow/Thigh = 16/9, CCR = 16 MHz / (25 * 400k) = 1 (minimum allowed) */
	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_FM4K, I2C_FM_DUTY_16_9, SLAVE_ADDR);
	CHECK(I2C1->CCR == ((1U << I2C_CCR_FS) | (1U << I2C_CCR_DUTY) | 1U),
	      "FM 400k duty 16/9: CCR expected 0xC001");
}

static void test_ack_after_enable(void) {
	/* The config asks for ACK. Real hardware: RM0390 says ACK is cleared by
	 * hardware when PE=0, so a value written before PE=1 may be lost. The
	 * user of the driver expects ACK on once the peripheral runs. */
	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_SM, I2C_FM_DUTY_2, SLAVE_ADDR);
	I2C_PeripheralControl(I2C1, DRV_ENABLE);

	CHECK(I2C1->CR1 & (1U << I2C_CR1_PE),  "PE not set by PeripheralControl(DRV_ENABLE)");
	CHECK(I2C1->CR1 & (1U << I2C_CR1_ACK), "Config.ACKControl=DRV_ENABLE, but ACK is 0 after enable "
	                                      "(ACK written while PE=0 is lost)");

	I2C_PeripheralControl(I2C1, DRV_DISABLE);
	CHECK(!(I2C1->CR1 & (1U << I2C_CR1_PE)), "PE not cleared by PeripheralControl(DRV_DISABLE)");
}

static void test_manage_acking(void) {
	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_SM, I2C_FM_DUTY_2, SLAVE_ADDR);
	I2C_PeripheralControl(I2C1, DRV_ENABLE);

	I2C_ManageAcking(I2C1, I2C_ACK_ENABLE);
	CHECK(I2C1->CR1 & (1U << I2C_CR1_ACK), "ManageAcking(DRV_ENABLE) did not set ACK");

	I2C_ManageAcking(I2C1, I2C_ACK_DISABLE);
	CHECK(!(I2C1->CR1 & (1U << I2C_CR1_ACK)), "ManageAcking(DRV_DISABLE) did not clear ACK");
	CHECK(I2C1->CR1 & (1U << I2C_CR1_PE),      "ManageAcking changed PE");

	I2C_PeripheralControl(I2C1, DRV_DISABLE);
}

static void test_nvic_config(void) {
	/* I2C1_EV = 31 (ISER0 bit 31), I2C3_ER = 73 (ISER2 bit 9) */
	static const uint8_t irqs[] = { IRQ_NO_I2C1_EV, IRQ_NO_I2C1_ER, IRQ_NO_I2C3_EV, IRQ_NO_I2C3_ER };

	for (uint32_t i = 0; i < sizeof(irqs); i++) {
		I2C_IRQInterruptConfig(irqs[i], DRV_ENABLE);
		CHECK(test_nvic_is_enabled(irqs[i]), "I2C IRQ not enabled in NVIC");

		I2C_IRQPriorityConfig(irqs[i], NVIC_IRQ_PRI10);
		CHECK(test_nvic_priority(irqs[i]) == NVIC_IRQ_PRI10, "I2C IRQ priority not 10");

		I2C_IRQInterruptConfig(irqs[i], DRV_DISABLE);
		CHECK(!test_nvic_is_enabled(irqs[i]), "I2C IRQ not disabled in NVIC");
	}
}

/* ========================================================================== */
/*   [LOOP] I2C1 master <-> I2C3 slave                                        */
/* ========================================================================== */

/* Checks the wires, then brings up master and slave. Returns 0 (and SKIPs)
 * if the wiring is missing or the bus is stuck. */
static int loop_setup(void) {
	int scl = test_wire_connected(GPIOB, 8, GPIOA, 8);
	int sda = test_wire_connected(GPIOB, 9, GPIOC, 9);

	if (!scl || !sda) {
		SKIP(!scl ? "wire PB8 (D15) <-> PA8 (D7) missing" : "wire PB9 (D14) <-> PC9 (CN10-1) missing");
		return 0;
	}

	i2c_gpio_init();
	reset_events();

	i2c_setup(&i2c1, I2C1, I2C_SCL_SPEED_SM, I2C_FM_DUTY_2, MASTER_OWN_ADDR);
	i2c_setup(&i2c3, I2C3, I2C_SCL_SPEED_SM, I2C_FM_DUTY_2, SLAVE_ADDR);

	I2C_PeripheralControl(I2C1, DRV_ENABLE);
	I2C_PeripheralControl(I2C3, DRV_ENABLE);

	/* ACK set again after PE=1, so the transfer tests do not depend on the
	 * i2c_ack_after_enable result. That test reports the ACK problem alone. */
	I2C_ManageAcking(I2C1, I2C_ACK_ENABLE);
	I2C_ManageAcking(I2C3, I2C_ACK_ENABLE);

	/* The driver has no "slave enable events" API yet, so the slave arms its
	 * interrupt sources directly. */
	I2C3->CR2 |= (1U << I2C_CR2_ITEVTEN) | (1U << I2C_CR2_ITBUFEN) | (1U << I2C_CR2_ITERREN);

	I2C_IRQPriorityConfig(IRQ_NO_I2C1_EV, NVIC_IRQ_PRI10);
	I2C_IRQPriorityConfig(IRQ_NO_I2C1_ER, NVIC_IRQ_PRI10);
	I2C_IRQPriorityConfig(IRQ_NO_I2C3_EV, NVIC_IRQ_PRI9);
	I2C_IRQPriorityConfig(IRQ_NO_I2C3_ER, NVIC_IRQ_PRI9);
	I2C_IRQInterruptConfig(IRQ_NO_I2C1_EV, DRV_ENABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C1_ER, DRV_ENABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C3_EV, DRV_ENABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C3_ER, DRV_ENABLE);

	if (I2C_IsBusy(I2C1)) {
		CHECK(0, "bus BUSY right after enable: SDA or SCL held low (pull-ups missing?)");
		return 0;
	}
	return 1;
}

static void loop_teardown(void) {
	I2C_IRQInterruptConfig(IRQ_NO_I2C1_EV, DRV_DISABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C1_ER, DRV_DISABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C3_EV, DRV_DISABLE);
	I2C_IRQInterruptConfig(IRQ_NO_I2C3_ER, DRV_DISABLE);
	/* RCC reset also frees a bus that a half-finished transfer left stuck */
	I2C_DeInit(I2C1);
	I2C_DeInit(I2C3);
}

static void test_loop_it_master_write(void) {
	static uint8_t tx[] = { 0x10, 0xA5, 0x5A, 0x00, 0xFF };

	if (!loop_setup()) {
		return;
	}

	CHECK(I2C_MasterSendDataIT(&i2c1, tx, sizeof(tx), SLAVE_ADDR, I2C_DISABLE_SR) == DRV_OK,
	      "MasterSendDataIT rejected on idle handle");

	CHECK(test_wait_count(&m_tx_cmplt, 1, TEST_TIMEOUT_MS), "no I2C_EVENT_TX_CMPLT on master (timeout)");
	CHECK(m_af == 0, "master got AF: slave did not ACK its address or data");
	CHECK(test_wait_count(&s_stop, 1, TEST_TIMEOUT_MS), "slave did not see STOP");

	CHECK(s_rx_len == sizeof(tx), "slave received wrong number of bytes");
	CHECK(memcmp((const void *)s_rx_buf, tx, sizeof(tx)) == 0, "slave received data differs");
	CHECK(i2c1.TxRxState == I2C_READY, "master state not back to I2C_READY");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus still BUSY after STOP");

	loop_teardown();
}

static void test_loop_it_master_read(void) {
	static uint8_t rx[4];
	static uint8_t rx1[1];

	if (!loop_setup()) {
		return;
	}

	memcpy((void *)s_tx_buf, "WXYZ", 4);
	memset(rx, 0, sizeof(rx));

	CHECK(I2C_MasterReceiveDataIT(&i2c1, rx, sizeof(rx), SLAVE_ADDR, I2C_DISABLE_SR) == DRV_OK,
	      "MasterReceiveDataIT rejected on idle handle");
	CHECK(test_wait_count(&m_rx_cmplt, 1, TEST_TIMEOUT_MS), "no I2C_EVENT_RX_CMPLT (4 bytes)");
	CHECK(memcmp(rx, "WXYZ", 4) == 0, "master read data differs (4 bytes)");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus still BUSY after read");

	/* Single byte read has its own ACK/STOP sequence in RM0390 */
	s_tx_idx = 0;
	s_tx_buf[0] = 0x3C;
	rx1[0] = 0;
	CHECK(I2C_MasterReceiveDataIT(&i2c1, rx1, 1, SLAVE_ADDR, I2C_DISABLE_SR) == DRV_OK,
	      "MasterReceiveDataIT rejected (1 byte)");
	CHECK(test_wait_count(&m_rx_cmplt, 2, TEST_TIMEOUT_MS), "no I2C_EVENT_RX_CMPLT (1 byte)");
	CHECK(rx1[0] == 0x3C, "master read data differs (1 byte)");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus still BUSY after 1-byte read");

	CHECK(I2C1->CR1 & (1U << I2C_CR1_ACK), "ACK not restored after read (next read would NACK byte 1)");

	loop_teardown();
}

static void test_loop_it_nack_absent_slave(void) {
	static uint8_t tx[] = { 0x01 };

	if (!loop_setup()) {
		return;
	}

	CHECK(I2C_MasterSendDataIT(&i2c1, tx, sizeof(tx), ABSENT_ADDR, I2C_DISABLE_SR) == DRV_OK,
	      "MasterSendDataIT rejected on idle handle");

	CHECK(test_wait_count(&m_af, 1, TEST_TIMEOUT_MS), "no I2C_ERROR_AF for an address nobody owns");
	CHECK(m_tx_cmplt == 0, "TX_CMPLT reported although the address was NACKed");
	CHECK(i2c1.TxRxState == I2C_READY, "master state not back to I2C_READY after AF");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus not released after AF (STOP missing)");

	loop_teardown();
}

static void test_loop_blocking_register_read(void) {
	uint8_t reg = 0x20;
	uint8_t rx[3] = { 0 };

	if (!loop_setup()) {
		return;
	}

	memcpy((void *)s_tx_buf, "\x11\x22\x33", 3);

	/* Typical sensor read: write register address, repeated START, read 3 bytes */
	CHECK(I2C_MasterSendData(&i2c1, &reg, 1, SLAVE_ADDR, I2C_ENABLE_SR, TEST_TIMEOUT_MS) == DRV_OK,
	      "blocking write of the register address failed");
	CHECK(I2C_MasterReceiveData(&i2c1, rx, sizeof(rx), SLAVE_ADDR, I2C_DISABLE_SR, TEST_TIMEOUT_MS) == DRV_OK,
	      "blocking read after repeated START failed");

	CHECK(s_rx_len == 1 && s_rx_buf[0] == reg, "slave did not get the register byte");
	CHECK(rx[0] == 0x11 && rx[1] == 0x22 && rx[2] == 0x33, "blocking read data differs");
	CHECK(test_wait_count(&s_stop, 1, TEST_TIMEOUT_MS), "no STOP after the read");
	CHECK(s_stop == 1, "STOP seen between write and read: repeated START not used");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus still BUSY");

	loop_teardown();
}

static void test_loop_blocking_nack_absent_slave(void) {
	uint8_t data = 0x01;
	uint8_t rx = 0;

	if (!loop_setup()) {
		return;
	}

	/* Nobody owns ABSENT_ADDR: the address byte gets a NACK. The call must stop
	 * at once with DRV_ERROR (not wait for the timeout) and free the bus. */
	uint32_t c0 = test_cycles();
	DRV_Status_t st_write = I2C_MasterSendData(&i2c1, &data, 1, ABSENT_ADDR, I2C_DISABLE_SR, TEST_TIMEOUT_MS);
	uint32_t ms = (test_cycles() - c0) / 16000U;

	CHECK(st_write == DRV_ERROR, "write to absent slave did not return DRV_ERROR");
	CHECK(ms < 10U, "NACK not detected early: call waited for the timeout");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus not released after NACK");
	CHECK(!(I2C1->SR1 & (1U << I2C_SR1_AF)), "AF flag left set after the failed write");

	CHECK(I2C_MasterReceiveData(&i2c1, &rx, 1, ABSENT_ADDR, I2C_DISABLE_SR, TEST_TIMEOUT_MS) == DRV_ERROR,
	      "read from absent slave did not return DRV_ERROR");
	CHECK(I2C1->CR1 & (1U << I2C_CR1_ACK), "ACK not restored after the failed read");
	CHECK(test_wait_reg(&I2C1->SR2, 1U << I2C_SR2_BUSY, 0, TEST_TIMEOUT_MS), "bus not released after failed read");

	loop_teardown();
}

void test_suite_i2c(void) {
	test_suite_begin("i2c");

	test_run(test_clock_control,      "i2c_clock_control");
	test_run(test_init_standard_100k, "i2c_init_standard_100k");
	test_run(test_init_fast_400k,     "i2c_init_fast_400k");
	test_run(test_ack_after_enable,   "i2c_ack_after_enable");
	test_run(test_manage_acking,      "i2c_manage_acking");
	test_run(test_nvic_config,        "i2c_nvic_config");

	test_run(test_loop_it_master_write,        "i2c_loop_it_master_write");
	test_run(test_loop_it_master_read,         "i2c_loop_it_master_read");
	test_run(test_loop_it_nack_absent_slave,   "i2c_loop_it_nack_absent_slave");
	test_run(test_loop_blocking_register_read, "i2c_loop_blocking_register_read");
	test_run(test_loop_blocking_nack_absent_slave, "i2c_loop_blocking_nack_absent_slave");

	I2C_DeInit(I2C1);
	I2C_DeInit(I2C3);
	I2C_PeriClockControl(I2C1, DRV_DISABLE);
	I2C_PeriClockControl(I2C3, DRV_DISABLE);
}
