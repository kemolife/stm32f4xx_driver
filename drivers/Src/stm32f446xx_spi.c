/*
 * stm32f446xx_spi.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 */

#include <stddef.h>
#include "stm32f446xx.h"

static void spi_tx_interrupt_handle(SPI_Handle_t *pSPIHandle);
static void spi_rx_interrupt_handle(SPI_Handle_t *pSPIHandle);
static void spi_ovr_interrupt_handle(SPI_Handle_t *pSPIHandle);

/******************************************************************************************
 * @fn                 - SPI_PeriClockControl
 *
 * @brief              - This function enables or disables peripheral clock for the given SPIx
 *
 * @param[in]          - base address of the spi peripheral
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, DRV_State_t State) {
	if (State == DRV_ENABLE) {
		if (pSPIx == SPI1) {
			SPI1_PCLK_EN();
		} else if (pSPIx == SPI2) {
			SPI2_PCLK_EN();
		} else if (pSPIx == SPI3) {
			SPI3_PCLK_EN();
		} else if (pSPIx == SPI4) {
			SPI4_PCLK_EN();
		}
	}else {
		if (pSPIx == SPI1) {
			SPI1_PCLK_DI();
		} else if (pSPIx == SPI2) {
			SPI2_PCLK_DI();
		} else if (pSPIx == SPI3) {
			SPI3_PCLK_DI();
		} else if (pSPIx == SPI4) {
			SPI4_PCLK_DI();
		}
	}
}

/******************************************************************************************
 * @fn                 - SPI_Init
 *
 * @brief              - Initializes the given SPIx with specified configurations
 *
 * @param[in]          - pointer to the SPI Handle structure
 *
 * @return             - none
 *
 * @Note               - writes the whole CR1, so call it while SPE is 0, before
 *                       SPI_PeripheralControl(..., DRV_ENABLE). The clock must already be on
 *
 ******************************************************************************************/
void SPI_Init(SPI_Handle_t *pSPIHandle){
	uint32_t temp = 0;

	// 1. Configure device mode (MSTR)
	temp |= (uint32_t)(pSPIHandle->Config.DeviceMode & 0x01) << SPI_CR1_MSTR;

	// 2. Configure the bus (BIDIMODE, RXONLY). temp starts at 0, so only the
	//    bits that must be 1 are set here.
	if (pSPIHandle->Config.BusConfig == SPI_BUS_CONFIG_HALF_DUPLEX) {
		// 1-line bidirectional data mode
		temp |= (1U << SPI_CR1_BIDIMODE);
	} else if (pSPIHandle->Config.BusConfig == SPI_BUS_CONFIG_SIMPLEX_RXONLY) {
		// 2-line unidirectional mode with the transmitter off
		temp |= (1U << SPI_CR1_RXONLY);
	}
	// SPI_BUS_CONFIG_FULL_DUPLEX: BIDIMODE = 0, RXONLY = 0

	// 3. Configure SclkSpeed (BR[2:0])
	temp |= (uint32_t)(pSPIHandle->Config.SclkSpeed & 0x07) << SPI_CR1_BR;

	// 4. Configure DFF (data frame format, 8 or 16 bit)
	temp |= (uint32_t)(pSPIHandle->Config.DFF & 0x01) << SPI_CR1_DFF;

	// 5. Configure CPOL (clock polarity)
	temp |= (uint32_t)(pSPIHandle->Config.CPOL & 0x01) << SPI_CR1_CPOL;

	// 6. Configure CPHA (clock phase)
	temp |= (uint32_t)(pSPIHandle->Config.CPHA & 0x01) << SPI_CR1_CPHA;

	// 7. Configure SSM (software slave management)
	temp |= (uint32_t)(pSPIHandle->Config.SSM & 0x01) << SPI_CR1_SSM;

	// 8. With SSM enabled the NSS level is driven by SSI. In master mode SSI must
	//    be held high, otherwise NSS reads low and the hardware raises a mode
	//    fault (MODF), which clears MSTR and SPE.
	if (pSPIHandle->Config.SSM == SPI_SSM_EN &&
	    pSPIHandle->Config.DeviceMode == SPI_DEVICE_MODE_MASTER) {
		temp |= (1U << SPI_CR1_SSI);
	}

	pSPIHandle->Instance->CR1 = temp;

	// 9. Hardware NSS in master mode: SSOE lets the master drive NSS low,
	//    otherwise NSS floats and the hardware raises a mode fault (MODF).
	if (pSPIHandle->Config.SSM == SPI_SSM_DI &&
		pSPIHandle->Config.DeviceMode == SPI_DEVICE_MODE_MASTER) {
		pSPIHandle->Instance->CR2 |= (1U << SPI_CR2_SSOE);
	}
}

/******************************************************************************************
 * @fn                 - SPI_PeripheralControl
 *
 * @brief              - Enables or disables the SPI peripheral (CR1 SPE bit)
 *
 * @param[in]          - base address of the SPI peripheral
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - call this after SPI_Init. Before disabling, wait until
 *                       SPI_IsBusy() returns 0, or the last frame is cut
 *
 ******************************************************************************************/
void SPI_PeripheralControl(SPI_RegDef_t *pSPIx, DRV_State_t State) {
	if (State == DRV_ENABLE) {
		pSPIx->CR1 |= (1U << SPI_CR1_SPE);
	} else {
		pSPIx->CR1 &= ~(1U << SPI_CR1_SPE);
	}
}

/******************************************************************************************
 * @fn                 - SPI_IsBusy
 *
 * @brief              - Reports whether the SPI is still shifting a frame
 *
 * @param[in]          - base address of the SPI peripheral
 *
 * @return             - 1 while BSY is set, 0 when idle
 *
 * @Note               - after the last SPI_SendData, wait for 0 before disabling the
 *                       peripheral or releasing chip select
 *
 ******************************************************************************************/
int SPI_IsBusy(SPI_RegDef_t *pSPIx) {
	return (pSPIx->SR >> SPI_SR_BSY) & 1U;
}

/******************************************************************************************
 * @fn                 - SPI_DeInit
 *
 * @brief              - Resets all registers of a given SPIx back to their reset values
 *
 * @param[in]          - base address of the SPI peripheral
 *
 * @return             - none
 *
 * @Note               - Uses the RCC peripheral reset registers
 *
 ******************************************************************************************/
void SPI_DeInit(SPI_RegDef_t *pSPIx) {
	if (pSPIx == SPI1) {
		SPI1_REG_RESET();
	} else if (pSPIx == SPI2) {
		SPI2_REG_RESET();
	} else if (pSPIx == SPI3) {
		SPI3_REG_RESET();
	} else if (pSPIx == SPI4) {
		SPI4_REG_RESET();
	}
}

/******************************************************************************************
 * @fn                 - SPI_SendData
 *
 * @brief              - Sends Len bytes over the SPI bus
 *
 * @param[in]          - base address of the SPI peripheral
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of bytes to send (must be even in 16-bit mode)
 * @param[in]          - maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 *
 * @return             - DRV_OK, DRV_ERROR (bad argument) or DRV_TIMEOUT
 *
 * @Note               - blocking call. It returns when the last byte is in the transmit
 *                       buffer, not when it has left the wire: check SPI_IsBusy() before
 *                       disabling the peripheral. Received bytes are not read here, so in
 *                       full duplex read them (SPI_ReceiveData) or OVR will be set.
 *                       TXE never comes back while SPE is 0, so a forgotten
 *                       SPI_PeripheralControl(..., DRV_ENABLE) ends in DRV_TIMEOUT
 *
 ******************************************************************************************/
DRV_Status_t SPI_SendData(SPI_RegDef_t *pSPIx, uint8_t *pTxBuffer, uint32_t Len, uint32_t Timeout) {
	if (Len == 0 || pTxBuffer == NULL) {
		return DRV_ERROR;
	}

	uint8_t dff_bit = (pSPIx->CR1 >> SPI_CR1_DFF) & 0x01;

	// Safety check: 16-bit mode requires an even number of bytes
	if (dff_bit == SPI_DFF_16BITS && (Len % 2 != 0)) {
		return DRV_ERROR;
	}

	uint32_t start = SYSTICK_GetTick();

	while(Len > 0) {
		while (!(pSPIx->SR & (1U << SPI_SR_TXE))) {
			if (SYSTICK_IsTimeout(start, Timeout)) {
				return DRV_TIMEOUT;
			}
		}

		if (dff_bit == SPI_DFF_8BITS) {
			*((volatile uint8_t*)&pSPIx->DR) = *pTxBuffer;
			pTxBuffer++;
			Len--;
		} else if (dff_bit == SPI_DFF_16BITS){
			pSPIx->DR = *(uint16_t *)pTxBuffer;
			pTxBuffer += 2;
			Len -= 2;
		}
	}

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - SPI_ReceiveData
 *
 * @brief              - Receives Len bytes from the SPI bus
 *
 * @param[in]          - base address of the SPI peripheral
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of bytes to receive (must be even in 16-bit mode)
 * @param[in]          - maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 *
 * @return             - DRV_OK, DRV_ERROR (bad argument) or DRV_TIMEOUT
 *
 * @Note               - blocking call. A master only receives while it clocks, so in
 *                       master mode send one dummy byte per byte you want to receive.
 *                       A slave whose master never clocks ends in DRV_TIMEOUT
 *
 ******************************************************************************************/
DRV_Status_t SPI_ReceiveData(SPI_RegDef_t *pSPIx, uint8_t *pRxBuffer, uint32_t Len, uint32_t Timeout) {
	if (Len == 0 || pRxBuffer == NULL) {
		return DRV_ERROR;
	}

	uint8_t dff_bit = (pSPIx->CR1 >> SPI_CR1_DFF) & 0x01;

	// Safety check: 16-bit mode requires an even number of bytes
	if (dff_bit == SPI_DFF_16BITS && (Len % 2 != 0)) {
		return DRV_ERROR;
	}

	uint32_t start = SYSTICK_GetTick();

	while(Len > 0) {
		while (!(pSPIx->SR & (1U << SPI_SR_RXNE))) {
			if (SYSTICK_IsTimeout(start, Timeout)) {
				return DRV_TIMEOUT;
			}
		}

		if (dff_bit == SPI_DFF_8BITS) {
			*pRxBuffer = *((volatile uint8_t*)&pSPIx->DR);
			pRxBuffer++;
			Len--;
		} else if (dff_bit == SPI_DFF_16BITS){
			*(uint16_t *)pRxBuffer = pSPIx->DR;
			pRxBuffer += 2;
			Len -= 2;
		}
	}

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - SPI_IRQInterruptConfig
 *
 * @brief              - Enables or disables the SPI interrupt line in the NVIC
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQInterruptConfig, kept so the SPI API is complete
 *
 ******************************************************************************************/
void SPI_IRQInterruptConfig(uint8_t IRQNumber, DRV_State_t State) {
	NVIC_IRQInterruptConfig(IRQNumber, State);
}

/******************************************************************************************
 * @fn                 - SPI_IRQPriorityConfig
 *
 * @brief              - Sets the priority of the SPI interrupt line
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - priority 0 (most urgent) .. 15 (least urgent)
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQPriorityConfig, kept so the SPI API is complete
 *
 ******************************************************************************************/
void SPI_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	NVIC_IRQPriorityConfig(IRQNumber, IRQPriority);
}

/******************************************************************************************
 * @fn                 - SPI_IRQHandling
 *
 * @brief              - Handles the SPI interrupt sources (TXE, RXNE, OVR)
 *
 * @param[in]          - pointer to the active SPI Handle structure
 *
 * @return             - none
 *
 * @Note               - call it from the SPIx_IRQHandler vector. Each flag is only handled
 *                       when its interrupt enable bit is set too
 *
 ******************************************************************************************/
void SPI_IRQHandling(SPI_Handle_t *pSPIHandle)
{
	uint32_t statusReg = pSPIHandle->Instance->SR;
	uint32_t controlReg2 = pSPIHandle->Instance->CR2;

	// 1. Check for TXE (Transmit buffer empty) interrupt
	uint8_t txe_flag = (statusReg & (1 << SPI_SR_TXE));        // SR bit 1: TXE
	uint8_t txeie_bit = (controlReg2 & (1 << SPI_CR2_TXEIE));     // CR2 bit 7: TXEIE

	if (txe_flag && txeie_bit) {
		spi_tx_interrupt_handle(pSPIHandle);
	}

	// 2. Check for RXNE (Receive buffer not empty) interrupt
	uint8_t rxne_flag = (statusReg & (1 << SPI_SR_RXNE));       // SR bit 0: RXNE
	uint8_t rxneie_bit = (controlReg2 & (1 << SPI_CR2_RXNEIE));    // CR2 bit 6: RXNEIE

	if (rxne_flag && rxneie_bit) {
		spi_rx_interrupt_handle(pSPIHandle);
	}

	if (statusReg & (1 << SPI_SR_OVR) && controlReg2 & (1 << SPI_CR2_ERRIE)) {
		spi_ovr_interrupt_handle(pSPIHandle);
	}
}

/******************************************************************************************
 * @fn                 - SPI_SendDataIT
 *
 * @brief              - Starts an interrupt driven transmission
 *
 * @param[in]          - pointer to the SPI Handle structure
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of bytes to send
 *
 * @return             - DRV_OK when started, DRV_BUSY when a transmission is still
 *                       running (nothing changed), DRV_ERROR for a bad argument
 *
 * @Note               - the buffer must stay valid until the SPI_EVENT_TX_CMPLT callback
 *
 ******************************************************************************************/
DRV_Status_t SPI_SendDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pTxBuffer, uint32_t Len) {
	if (pTxBuffer == NULL || Len == 0) {
		return DRV_ERROR;
	}
	if (pSPIHandle->TxState == SPI_BUSY_IN_TX) {
		return DRV_BUSY;
	}

	pSPIHandle->pTxBuffer = pTxBuffer;
	pSPIHandle->TxLen = Len;

	pSPIHandle->TxState = SPI_BUSY_IN_TX;

	pSPIHandle->Instance->CR2 |= (1U << SPI_CR2_TXEIE);

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - SPI_ReceiveDataIT
 *
 * @brief              - Starts an interrupt driven reception
 *
 * @param[in]          - pointer to the SPI Handle structure
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of bytes to receive
 *
 * @return             - DRV_OK when started, DRV_BUSY when a reception is still
 *                       running (nothing changed), DRV_ERROR for a bad argument
 *
 * @Note               - the buffer must stay valid until the SPI_EVENT_RX_CMPLT callback.
 *                       A master must also send (SPI_SendDataIT) to generate the clock
 *
 ******************************************************************************************/
DRV_Status_t SPI_ReceiveDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pRxBuffer, uint32_t Len) {
	if (pRxBuffer == NULL || Len == 0) {
		return DRV_ERROR;
	}
	if (pSPIHandle->RxState == SPI_BUSY_IN_RX) {
		return DRV_BUSY;
	}

	pSPIHandle->pRxBuffer = pRxBuffer;
	pSPIHandle->RxLen = Len;

	pSPIHandle->RxState = SPI_BUSY_IN_RX;

	pSPIHandle->Instance->CR2 |= (1U << SPI_CR2_RXNEIE);

	return DRV_OK;
}

static void spi_tx_interrupt_handle(SPI_Handle_t *pSPIHandle) {
	uint8_t dff_bit = (pSPIHandle->Instance->CR1 >> SPI_CR1_DFF) & 0x01;

	if (pSPIHandle->TxLen > 0) {
		if (dff_bit == SPI_DFF_8BITS) {
			*((volatile uint8_t*)&pSPIHandle->Instance->DR) = *pSPIHandle->pTxBuffer;
			pSPIHandle->pTxBuffer++;
			pSPIHandle->TxLen--;
		} else if (dff_bit == SPI_DFF_16BITS){
			pSPIHandle->Instance->DR = *(uint16_t *)pSPIHandle->pTxBuffer;
			pSPIHandle->pTxBuffer += 2;
			pSPIHandle->TxLen -= 2;
		}
	}

	// If TxLen hits 0, turn off the TXEIE interrupt to stop entering this ISR loop
	if (pSPIHandle->TxLen == 0) {
		pSPIHandle->Instance->CR2 &= ~(1 << SPI_CR2_TXEIE);
		pSPIHandle->pTxBuffer = NULL;
		pSPIHandle->TxState = SPI_READY;

		// Call application callback
		SPI_ApplicationEventCallback(pSPIHandle, SPI_EVENT_TX_CMPLT);
	}
}

static void spi_rx_interrupt_handle(SPI_Handle_t *pSPIHandle) {
	uint8_t dff_bit = (pSPIHandle->Instance->CR1 >> SPI_CR1_DFF) & 0x01;

	if (pSPIHandle->RxLen > 0) {
		// Pull data from DR depending on DFF size
		if (dff_bit == SPI_DFF_8BITS) { // 8-bit
			*pSPIHandle->pRxBuffer = *((volatile uint8_t*)&pSPIHandle->Instance->DR);
			pSPIHandle->pRxBuffer++;
			pSPIHandle->RxLen--;
		} else { // 16-bit
			*(uint16_t*)pSPIHandle->pRxBuffer = pSPIHandle->Instance->DR;
			pSPIHandle->pRxBuffer += 2;
			pSPIHandle->RxLen -= 2;
		}
	}

	// If RxLen hits 0, turn off the RXNEIE interrupt and reset the state
	if (pSPIHandle->RxLen == 0) {
		pSPIHandle->Instance->CR2 &= ~(1 << SPI_CR2_RXNEIE);
		pSPIHandle->pRxBuffer = NULL;
		pSPIHandle->RxState = SPI_READY;

		// Optional: Call application callback if you have one configured
		SPI_ApplicationEventCallback(pSPIHandle, SPI_EVENT_RX_CMPLT);
	}
}

/*
 * Overrun means a frame arrived while RXNE was still set, so a byte was lost.
 * The flag is cleared by reading DR and then SR, in that order.
 */
static void spi_ovr_interrupt_handle(SPI_Handle_t *pSPIHandle) {
	uint8_t temp;

	// Do not consume the byte if a reception is still in progress
	if (pSPIHandle->RxState != SPI_BUSY_IN_RX) {
		temp = *((volatile uint8_t*)&pSPIHandle->Instance->DR);
		temp = pSPIHandle->Instance->SR;
	}
	(void)temp;

	SPI_ApplicationEventCallback(pSPIHandle, SPI_EVENT_OVR_ERR);
}

__attribute__((weak)) void SPI_ApplicationEventCallback(SPI_Handle_t *pSPIHandle, uint8_t AppEv)
{
    // This is an empty placeholder.
    // The user can override this function in main.c without modifying the driver!
    (void)pSPIHandle;
    (void)AppEv;
}
