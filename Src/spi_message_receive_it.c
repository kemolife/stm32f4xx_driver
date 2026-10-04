/*
 * spi_message_receive_it.c
 *
 *  Created on: 12 Sept 2026
 *      Author: vitaliiantoniuk
 *
 * SPI2 master. Receives text messages pushed by a Raspberry Pi Pico 2 W and
 * prints them to the SWV / ITM console.
 *
 * The Pico is the SPI slave, so it cannot start a transfer. It raises an
 * attention line instead; the STM32 takes an EXTI on that pin and then clocks
 * the message out of the Pico.
 *
 *   PC6  <- Pico GPIO16   attention, rising edge means "message queued"
 *   PB12 -> Pico GPIO13   NSS  (hardware SSOE)
 *   PB13 -> Pico GPIO10   SCK
 *   PB14 <- Pico GPIO11   MISO
 *   PB15 -> Pico GPIO12   MOSI
 *
 * Wire format: one length byte, then that many ASCII bytes.
 *
 * CPHA=1 and 500 kHz are carried over from spi_tx_pico2w.c and are not
 * negotiable -- see the header comment there for why.
 */
#include <stdio.h>
#include <string.h>
#include "stm32f446xx.h"

#define SPI_TIMEOUT_MS  10U   // one byte at 500 kHz takes 16 us

#define ATTN_PIN        6
#define MAX_MSG_LEN     64

/* ---------- SWV / ITM printf retarget ------------------------------------ */
/*
 * This project has no CMSIS core headers, so the three trace registers are
 * addressed directly. Output only appears while a debugger has enabled
 * tracing -- see the note at the bottom of this file.
 */
#define ITM_STIM0       (*(volatile uint32_t*)0xE0000000)
#define ITM_TER         (*(volatile uint32_t*)0xE0000E00)
#define DEMCR           (*(volatile uint32_t*)0xE000EDFC)
#define DEMCR_TRCENA    (1UL << 24)

static void ITM_SendChar(char ch) {
	if (!(DEMCR & DEMCR_TRCENA)) { return; }   // trace not enabled by debugger
	if (!(ITM_TER & 1UL))        { return; }   // stimulus port 0 disabled

	while (ITM_STIM0 == 0);                    // wait for port to accept a byte
	*(volatile uint8_t*)&ITM_STIM0 = (uint8_t)ch;
}

/* Overrides the weak _write in syscalls.c, so printf lands on SWV. */
int _write(int file, char *ptr, int len) {
	(void)file;
	for (int i = 0; i < len; i++) {
		ITM_SendChar(ptr[i]);
	}
	return len;
}

/* ---------- shared state ------------------------------------------------- */

static SPI_Handle_t SPI2Handle;

static volatile uint8_t attn_pending = 0;   // set by EXTI, cleared by main
static volatile uint8_t rx_complete  = 0;   // set by the SPI RX-done callback
static volatile uint8_t ovr_error    = 0;

static uint8_t rxBuffer[MAX_MSG_LEN + 1];
static uint8_t txDummy[MAX_MSG_LEN];

/* ---------- init --------------------------------------------------------- */

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

	gpioSPI.Config.PinNumber = 15;   // MOSI
	GPIO_Init(&gpioSPI);

	gpioSPI.Config.PinNumber = 14;   // MISO
	GPIO_Init(&gpioSPI);

	gpioSPI.Config.PinNumber = 13;   // SCK
	GPIO_Init(&gpioSPI);

	gpioSPI.Config.PinNumber = 12;   // NSS
	GPIO_Init(&gpioSPI);
}

static void SPI_ConfigInit(void) {
	memset(&SPI2Handle, 0, sizeof(SPI_Handle_t));

	SPI2Handle.Instance = SPI2;
	SPI2Handle.Config.DeviceMode = SPI_DEVICE_MODE_MASTER;
	SPI2Handle.Config.BusConfig = SPI_BUS_CONFIG_FULL_DUPLEX;
	SPI2Handle.Config.DFF = SPI_DFF_8BITS;
	SPI2Handle.Config.SclkSpeed = SPI_SCLK_SPEED_DIV32;  // 500 kHz
	SPI2Handle.Config.CPOL = SPI_CPOL_LOW;
	SPI2Handle.Config.CPHA = SPI_CPHA_HIGH;
	SPI2Handle.Config.SSM = SPI_SSM_DI;                  // hardware NSS

	SPI_PeriClockControl(SPI2, DRV_ENABLE);
	SPI_Init(&SPI2Handle);

	// SPI_Init does not touch ERRIE; enable it so overruns reach the callback
	SPI2->CR2 |= (1 << SPI_CR2_ERRIE);

	SPI_IRQInterruptConfig(IRQ_NO_SPI2, DRV_ENABLE);
}

static void GPIO_AttnInit(void) {
	GPIO_Handle_t gpioAttn;

	memset(&gpioAttn, 0, sizeof(GPIO_Handle_t));

	gpioAttn.Instance = GPIOC;
	gpioAttn.Config.PinNumber = ATTN_PIN;
	gpioAttn.Config.Mode = GPIO_MODE_IT_RT;   // Pico drives it high
	gpioAttn.Config.PuPdControl = GPIO_NOT_PUPD;
	gpioAttn.Config.Speed = GPIO_SPEED_FAST;

	GPIO_PeriClockControl(GPIOC, DRV_ENABLE);
	GPIO_Init(&gpioAttn);

	GPIO_IRQPriorityConfig(IRQ_NO_EXTI9_5, NVIC_IRQ_PRI15);
	GPIO_IRQInterruptConfig(IRQ_NO_EXTI9_5, DRV_ENABLE);
}

/* ---------- interrupt handlers ------------------------------------------- */

void EXTI9_5_IRQHandler(void) {
	GPIO_IRQHandling(ATTN_PIN);
	attn_pending = 1;          // no SPI work in the ISR
}

void SPI2_IRQHandler(void) {
	SPI_IRQHandling(&SPI2Handle);
}

void SPI_ApplicationEventCallback(SPI_Handle_t *pSPIHandle, uint8_t AppEv) {
	(void)pSPIHandle;

	if (AppEv == SPI_EVENT_RX_CMPLT) {
		rx_complete = 1;
	} else if (AppEv == SPI_EVENT_OVR_ERR) {
		ovr_error = 1;
	}
}

/* ---------- message reception -------------------------------------------- */

/*
 * Clocks one byte out of the slave. In master mode the clock only runs while
 * a byte is being shifted out, so every read costs a dummy write.
 */
static uint8_t SPI_ExchangeByte(uint8_t out) {
	uint8_t in = 0;

	SPI_SendData(SPI2, &out, 1, SPI_TIMEOUT_MS);
	SPI_ReceiveData(SPI2, &in, 1, SPI_TIMEOUT_MS);

	return in;
}

static void SPI_ReceiveMessage(void) {
	uint8_t len = SPI_ExchangeByte(0xFF);

	if (len == 0 || len > MAX_MSG_LEN) {
		printf("[SPI] bad length byte 0x%02X\n", len);
		return;
	}

	memset(txDummy, 0xFF, len);
	rx_complete = 0;
	ovr_error = 0;

	// RX armed before TX: enabling TXEIE fires the ISR immediately
	SPI_ReceiveDataIT(&SPI2Handle, rxBuffer, len);
	SPI_SendDataIT(&SPI2Handle, txDummy, len);

	while (!rx_complete && !ovr_error);

	if (ovr_error) {
		printf("[SPI] overrun, message dropped\n");
		return;
	}

	rxBuffer[len] = '\0';
	printf("[SPI] %u bytes: \"%s\"\n", (unsigned)len, rxBuffer);
}

int main(void) {
	SPI_GPIO_ConfigInit();
	SPI_ConfigInit();
	GPIO_AttnInit();

	SPI_PeripheralControl(SPI2, DRV_ENABLE);

	printf("[SPI] master ready, waiting for Pico attention on PC%d\n", ATTN_PIN);

	while (1) {
		if (attn_pending) {
			attn_pending = 0;
			SPI_ReceiveMessage();
		}
	}
}

/*
 * Viewing the output:
 *   Debug from STM32CubeIDE, not probe-rs -- SWV needs the debugger to turn on
 *   trace. In the debug configuration set the core clock to 16 MHz (this
 *   project runs on HSI with no PLL), enable SWV, then open
 *   Window > Show View > SWV > SWV ITM Data Console, enable port 0 and start
 *   trace. PB3 is wired to the ST-LINK SWO pin on the Nucleo board.
 */
