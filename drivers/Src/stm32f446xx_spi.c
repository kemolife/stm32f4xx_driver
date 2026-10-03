/*
 * stm32f446xx.c
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
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi) {
	if (EnorDi == ENABLE) {
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
 * @Note               - none
 *
 ******************************************************************************************/
void SPI_Init(SPI_Handle_t *pSPIHandle){
	uint32_t temp = 0;

	// 1. Configure device mode MSTR (Master selection - Bit 2)
	temp |= (pSPIHandle->Config.DeviceMode & 0x01) << 2;

	// configure bus BIDIMODE
	if (pSPIHandle->Config.BusConfig == SPI_BUS_CONFIG_FULL_DUPLEX) {
		// BIDIMODE must be clear (0: 2-line unidirectional data mode selected)
		temp &= ~(1 << 15);
		temp &= ~(1 << 10);
	}else if (pSPIHandle->Config.BusConfig == SPI_BUS_CONFIG_HALF_DUPLEX) {
		// BIDIMODE must be set (1-line bidirectional data mode selected)
		temp |= (1 << 15);
		temp &= ~(1 << 10);
	}else if (pSPIHandle->Config.BusConfig == SPI_BUS_CONFIG_SIMPLEX_RXONLY) {
		// BIDIMODE must be clear (0: 2-line unidirectional data mode selected)
		temp &= ~(1 << 15);
		// set RXONLY (Receive only mode enable)
		temp |= (1 << 10);
	}

	// 3. Configure SclkSpeed (BR[2:0] - Bits 5:3)
	temp |= (pSPIHandle->Config.SclkSpeed & 0x07) << 3;

	// 4. Configure DFF (Data frame format - Bit 11)
	temp |= (pSPIHandle->Config.DFF & 0x01) << 11;

	// 5. Configure CPOL (Clock polarity - Bit 1)
	temp |= (pSPIHandle->Config.CPOL & 0x01) << 1;

	// 6. Configure CPHA (Clock phase - Bit 0)
	temp |= (pSPIHandle->Config.CPHA & 0x01) << 0;

	// 7. Configure SSM (Software Slave Management - Bit 9)
	temp |= (pSPIHandle->Config.SSM & 0x01) << SPI_CR1_SSM;

	// 8. With SSM enabled the NSS level is driven by SSI. In master mode SSI must
	//    be held high, otherwise NSS reads low and the hardware raises a mode
	//    fault (MODF), which clears MSTR and SPE.
	if (pSPIHandle->Config.SSM == SPI_SSM_EN &&
	    pSPIHandle->Config.DeviceMode == SPI_DEVICE_MODE_MASTER) {
		temp |= (1 << SPI_CR1_SSI);
	}

	pSPIHandle->Instance->CR1 = temp;

	if (pSPIHandle->Config.SSM == SPI_SSM_DI &&
		pSPIHandle->Config.DeviceMode == SPI_DEVICE_MODE_MASTER) {
		pSPIHandle->Instance->CR2 |= (1 << SPI_CR2_SSOE);
	}
}

void SPI_PeripheralControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi) {
	if (EnorDi == ENABLE) {
		pSPIx->CR1 |= (1 << 6);
	} else {
		pSPIx->CR1 &= ~(1 << 6);
	}
}

int SPI_IsBusy(SPI_RegDef_t *pSPIx) {
	return (pSPIx->SR >> SPI_SR_BSY) & 1;
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
 * @fn                 - GPIO_ReadFromInputPin
 *
 * @brief              - Reads the digital state of a specific pin on a given GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 * @param[in]          - pin number to read from (0 to 15)
 *
 * @return             - 0 or 1
 *
 * @Note               - this is a blocking call
 *
 ******************************************************************************************/
void SPI_SendData(SPI_RegDef_t *pSPIx, uint8_t *pTxBuffer, uint32_t Len) {
	if (Len == 0 || pTxBuffer == NULL) {
		return;
	}

	uint8_t dff_bit = (pSPIx->CR1 >> 11) & 0x01;

	// Safety check: 16-bit mode requires an even number of bytes
	if (dff_bit == SPI_DFF_16BITS && (Len % 2 != 0)) {
		return;
	}

	while(Len > 0) {
		while (!(pSPIx->SR & (1 << SPI_SR_TXE)));

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
}

/******************************************************************************************
 * @fn                 - GPIO_ReadFromInputPort
 *
 * @brief              - Reads the digital state of an entire GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 *
 * @return             - 16-bit value representing the entire port status
 *
 * @Note               - none
 *
 ******************************************************************************************/
void SPI_ReceiveData(SPI_RegDef_t *pSPIx, uint8_t *pRxBuffer, uint32_t Len) {
	if (Len == 0 || pRxBuffer == NULL) {
		return;
	}

	uint8_t dff_bit = (pSPIx->CR1 >> 11) & 0x01;

	// Safety check: 16-bit mode requires an even number of bytes
	if (dff_bit == SPI_DFF_16BITS && (Len % 2 != 0)) {
		return;
	}

	while(Len > 0) {
		while (!(pSPIx->SR & (1 << SPI_SR_RXNE)));

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
}

/******************************************************************************************
 * @fn                 - GPIO_IRQInterruptConfig
 *
 * @brief              - Enables or disables the interrupt processing for a given IRQ number in NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - Configures the ARM Cortex-M4 NVIC registers
 *
 ******************************************************************************************/
void SPI_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi)
{
    if (EnorDi == ENABLE)
    {
        if (IRQNumber < 32)
        {
            NVIC_ISER0 = (1U << IRQNumber);
        }
        else if (IRQNumber < 64)
        {
            NVIC_ISER1 = (1U << (IRQNumber - 32));
        }
        else if (IRQNumber < 96)
        {
            NVIC_ISER2 = (1U << (IRQNumber - 64));
        }
    }
    else // DISABLE
    {
        if (IRQNumber < 32)
        {
            NVIC_ICER0 = (1U << IRQNumber);
        }
        else if (IRQNumber < 64)
        {
            NVIC_ICER1 = (1U << (IRQNumber - 32));
        }
        else if (IRQNumber < 96)
        {
            NVIC_ICER2 = (1U << (IRQNumber - 64));
        }
    }
}

/******************************************************************************************
 * @fn                 - GPIO_IRQPriorityConfig
 *
 * @brief              - Configures the priority level of a given IRQ number in the NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - priority level value
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void SPI_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority)
{
    // Each IPR register handles 4 IRQs (8 bits per IRQ slot).
    // Cortex-M4 only uses the upper 4 bits of the 8-bit priority field.
    uint8_t shift_amount = ((IRQNumber % 4) * 8) + 4;

    if (IRQNumber < 4) {
        NVIC_IPR0 &= ~(0xF0U << ((IRQNumber % 4) * 8)); // Clear old priority MSBs
        NVIC_IPR0 |= ((uint32_t)IRQPriority << shift_amount);
    } else if (IRQNumber < 8) {
        NVIC_IPR1 &= ~(0xF0U << ((IRQNumber % 4) * 8));
        NVIC_IPR1 |= ((uint32_t)IRQPriority << shift_amount);
    } else if (IRQNumber < 12) {
        NVIC_IPR2 &= ~(0xF0U << ((IRQNumber % 4) * 8));
        NVIC_IPR2 |= ((uint32_t)IRQPriority << shift_amount);
    }
    // Target shared EXTI vector lines directly for optimization
    else if (IRQNumber == 23) { // EXTI9_5_IRQn
        NVIC_IPR5 &= ~(0xF0U << ((23 % 4) * 8));
        NVIC_IPR5 |= ((uint32_t)IRQPriority << shift_amount);
    } else if (IRQNumber == 40) { // EXTI15_10_IRQn
        NVIC_IPR10 &= ~(0xF0U << ((40 % 4) * 8));
        NVIC_IPR10 |= ((uint32_t)IRQPriority << shift_amount);
    }
}

/******************************************************************************************
 * @fn                 - GPIO_IRQHandling
 *
 * @brief              - Clears the pending interrupt status on the EXTI controller line
 *
 * @param[in]          - PinNumber: pin number processing the interrupt (0 to 15)
 *
 * @return             - none
 *
 * @Note               - Should be called inside the actual ISR vector handler.
 *                       Uses 'rc_w1' mechanics (Write-1-to-Clear).
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

uint8_t SPI_SendDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pTxBuffer, uint32_t Len) {
	uint8_t state = pSPIHandle->TxState;

	if (state == SPI_BUSY_IN_TX){ return state; }

	pSPIHandle->pTxBuffer = pTxBuffer;
	pSPIHandle->TxLen = Len;

	pSPIHandle->TxState = SPI_BUSY_IN_TX;

	pSPIHandle->Instance->CR2 |= 1 << SPI_CR2_TXEIE;

	return state;
}

uint8_t SPI_ReceiveDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pRxBuffer, uint32_t Len) {
	uint8_t state = pSPIHandle->RxState;

	if (state == SPI_BUSY_IN_RX){ return state; }

	pSPIHandle->pRxBuffer = pRxBuffer;
	pSPIHandle->RxLen = Len;

	pSPIHandle->RxState = SPI_BUSY_IN_RX;

	pSPIHandle->Instance->CR2 |= 1 << SPI_CR2_RXNEIE;

	return state;
}

void spi_tx_interrupt_handle(SPI_Handle_t *pSPIHandle) {
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

void spi_rx_interrupt_handle(SPI_Handle_t *pSPIHandle) {
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
void spi_ovr_interrupt_handle(SPI_Handle_t *pSPIHandle) {
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
}
