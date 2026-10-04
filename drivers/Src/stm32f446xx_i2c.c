/*
 * stm32f446xx_i2c.c
 *
 *  Created on: 13 Sept 2026
 *  Author: vitaliiantoniuk
 */

#include <stddef.h>
#include <stdint.h>
#include "stm32f446xx.h"

static void clear_ADDR_flag(I2C_Handle_t *pI2CHandle);
static DRV_Status_t wait_sr1(I2C_RegDef_t *pI2Cx, uint32_t bit, uint32_t start, uint32_t Timeout);
static DRV_Status_t master_abort(I2C_Handle_t *pI2CHandle, DRV_Status_t status);
static void I2C_MasterHandleTXEInterrupt(I2C_Handle_t *pI2CHandle);
static void I2C_MasterHandleRXNEInterrupt(I2C_Handle_t *pI2CHandle);
static void I2C_CloseSendData(I2C_Handle_t *pI2CHandle);
static void I2C_CloseReceiveData(I2C_Handle_t *pI2CHandle);

/* Clear ADDR with the mandatory read SR1 -> read SR2 sequence, without waiting.
 * Callers make sure ADDR is set first (ISR, or wait_sr1 in the blocking paths). */
static void clear_ADDR_flag(I2C_Handle_t *pI2CHandle) {
	uint32_t dummy_read;

	dummy_read = pI2CHandle->Instance->SR1;
	dummy_read = pI2CHandle->Instance->SR2;
	(void)dummy_read;
}

/* Waits until one SR1 flag is set, for the blocking master functions.
 * Returns DRV_ERROR at once when the slave answered NACK (AF), DRV_TIMEOUT
 * when the whole call ran out of time, DRV_OK when the flag came. */
static DRV_Status_t wait_sr1(I2C_RegDef_t *pI2Cx, uint32_t bit, uint32_t start, uint32_t Timeout) {
	while (!(pI2Cx->SR1 & (1U << bit))) {
		if (pI2Cx->SR1 & (1U << I2C_SR1_AF)) {
			return DRV_ERROR;
		}
		if (SYSTICK_IsTimeout(start, Timeout)) {
			return DRV_TIMEOUT;
		}
	}
	return DRV_OK;
}

/* Ends a failed blocking master transfer so the bus is usable again:
 * STOP releases SDA/SCL, AF is cleared (rc_w0), ACK goes back to the config. */
static DRV_Status_t master_abort(I2C_Handle_t *pI2CHandle, DRV_Status_t status) {
	pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_STOP);
	pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_AF);

	if (pI2CHandle->Config.ACKControl == I2C_ACK_ENABLE) {
		I2C_ManageAcking(pI2CHandle->Instance, I2C_ACK_ENABLE);
	}
	return status;
}

/******************************************************************************************
 * @fn                 - I2C_PeriClockControl
 *
 * @brief              - This function enables or disables peripheral clock for the given I2Cx
 *
 * @param[in]          - base address of the i2c peripheral
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - the clock must be enabled before any other register is touched
 *
 ******************************************************************************************/
void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, DRV_State_t State) {
    if (State == DRV_ENABLE) {
        if (pI2Cx == I2C1) {
            I2C1_PCLK_EN();
        } else if (pI2Cx == I2C2) {
            I2C2_PCLK_EN();
        } else if (pI2Cx == I2C3) {
            I2C3_PCLK_EN();
        }
    }else {
        if (pI2Cx == I2C1) {
            I2C1_PCLK_DI();
        } else if (pI2Cx == I2C2) {
            I2C2_PCLK_DI();
        } else if (pI2Cx == I2C3) {
            I2C3_PCLK_DI();
        }
    }
}

/******************************************************************************************
 * @fn                 - I2C_Init
 *
 * @brief              - Initializes the given I2Cx with specified configurations
 *
 * @param[in]          - pointer to the I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - FREQ, CCR and TRISE are only writable while PE is 0, so call this
 *                       before I2C_PeripheralControl(..., DRV_ENABLE)
 *
 ******************************************************************************************/
void I2C_Init(I2C_Handle_t *pI2CHandle) {
    // Must be at least 16-bit: CCR packs FS at bit 15 and DUTY at bit 14, and
    // OAR1 needs bit 14 held high. Use 32-bit to match the register width.
    uint32_t tempreg = 0;

    // config CR1
    tempreg |= pI2CHandle->Config.ACKControl << I2C_CR1_ACK;
    pI2CHandle->Instance->CR1 = tempreg;
    
	// configure PCLK
	tempreg = 0;
	tempreg |= RCC_GetPCLK1Value() / 1000000U;
	pI2CHandle->Instance->CR2 = (tempreg & 0x3F); // FREQ is [5:0]

	// configure own address
	tempreg = 0;
	tempreg |= (uint32_t)(pI2CHandle->Config.DeviceAddress & 0x7FU) << I2C_OAR1_ADD71;
	tempreg |= (1U << 14); // RM0390: bit 14 is reserved but must be kept at 1
	pI2CHandle->Instance->OAR1 = tempreg;

	// configure CCR
	tempreg = 0;
	uint32_t ccr_value = 0;
	if (pI2CHandle->Config.SCLSpeed == I2C_SCL_SPEED_SM) {
	    // Standard mode: Thigh = Tlow = CCR * TPCLK1, so one SCL period is 2*CCR
	    ccr_value = RCC_GetPCLK1Value() / (2U * pI2CHandle->Config.SCLSpeed);
		tempreg |= ((ccr_value << I2C_CCR_CCR) & 0xFFF);
	} else if (
	    pI2CHandle->Config.SCLSpeed == I2C_SCL_SPEED_FM2K ||
		pI2CHandle->Config.SCLSpeed == I2C_SCL_SPEED_FM4K			
	) {
	    tempreg |= (1U << I2C_CCR_FS);
		tempreg |= ((uint32_t)pI2CHandle->Config.FMDutyCycle << I2C_CCR_DUTY);
		if (pI2CHandle->Config.FMDutyCycle == I2C_FM_DUTY_2) {
		    // Tlow/Thigh = 2, so one SCL period spans 3*CCR
            ccr_value = RCC_GetPCLK1Value() / (3U * pI2CHandle->Config.SCLSpeed);
		} else {
		    // Tlow/Thigh = 16/9, so one SCL period spans 25*CCR
		    ccr_value = RCC_GetPCLK1Value() / (25U * pI2CHandle->Config.SCLSpeed);
		}
        tempreg |= ((ccr_value << I2C_CCR_CCR) & 0xFFF);
	}
	pI2CHandle->Instance->CCR = tempreg;

	// configure TRISE (max SCL rise time), must be written before PE=1
	// RM0390: TRISE = (Trise_max / TPCLK1) + 1
	//   SM: Trise_max = 1000 ns -> (FPCLK1 / 1 MHz) + 1
	//   FM: Trise_max =  300 ns -> (FPCLK1 * 300 ns) + 1
	tempreg = 0;
	if (pI2CHandle->Config.SCLSpeed <= I2C_SCL_SPEED_SM) {
		tempreg = (RCC_GetPCLK1Value() / 1000000U) + 1U;
	} else {
		// divide to MHz first so 45 MHz * 300 cannot overflow 32 bits
		tempreg = (((RCC_GetPCLK1Value() / 1000000U) * 300U) / 1000U) + 1U;
	}
	pI2CHandle->Instance->TRISE = (tempreg & 0x3FU); // TRISE is [5:0]
}

/******************************************************************************************
 * @fn                 - I2C_DeInit
 *
 * @brief              - Resets all registers of a given I2Cx back to their reset values
 *
 * @param[in]          - base address of the I2C peripheral
 *
 * @return             - none
 *
 * @Note               - drives the RCC reset line, so it clears PE and every config bit
 *
 ******************************************************************************************/
void I2C_DeInit(I2C_RegDef_t *pI2Cx) {
    if (pI2Cx == I2C1) {
        I2C1_REG_RESET();
    } else if (pI2Cx == I2C2) {
        I2C2_REG_RESET();
    } else if (pI2Cx == I2C3) {
        I2C3_REG_RESET();
    }
}

/******************************************************************************************
 * @fn                 - I2C_MasterSendData
 *
 * @brief              - Sends data to a slave device as bus master
 *
 * @param[in]          - pointer to the I2C Handle structure
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of bytes to send
 * @param[in]          - 7-bit slave address
 * @param[in]          - I2C_ENABLE_SR or I2C_DISABLE_SR
 * @param[in]          - maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 *
 * @return             - DRV_OK, DRV_ERROR (slave answered NACK, or bad argument) or
 *                       DRV_TIMEOUT (bus stuck, missing pull-ups, clock stretched too long)
 *
 * @Note               - blocking call. On DRV_ERROR and DRV_TIMEOUT a STOP is sent, so
 *                       the bus is free again for the next transfer
 *
 ******************************************************************************************/
DRV_Status_t I2C_MasterSendData(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr, uint32_t Timeout) {
	I2C_RegDef_t *pI2Cx = pI2CHandle->Instance;
	DRV_Status_t status;

	if (pTxBuffer == NULL && Len > 0U) {
		return DRV_ERROR;
	}

	uint32_t start = SYSTICK_GetTick();

	/* 1. Generate the START condition. The peripheral first waits for the bus to be
	 *    free, then pulls SDA low while SCL is still high. */
	pI2Cx->CR1 |= (1U << I2C_CR1_START);

	/* 2. Wait until SB is set, which confirms the START was placed on the bus.
	 *    While SB is set the hardware stretches SCL low, so the bus cannot move on
	 *    without us. SB is cleared by reading SR1 and then writing DR (step 3).
	 *    No SB in time: bus held by someone else, or no pull-ups. */
	status = wait_sr1(pI2Cx, I2C_SR1_SB, start, Timeout);
	if (status != DRV_OK) {
		return master_abort(pI2CHandle, status);
	}

	/* 3. Send the address phase: 7-bit slave address in bits [7:1] and the R/W bit
	 *    cleared in bit 0 to ask for a write. This DR write also clears SB. */
	pI2Cx->DR = (((uint32_t)SlaveAddr << 1) & ~(1U));

	/* 4. Wait for ADDR. It is set only after the slave acknowledged its address.
	 *    A missing or wrong slave answers NACK instead: AF is set and wait_sr1
	 *    returns DRV_ERROR at once. SCL stays stretched low until ADDR is cleared. */
	status = wait_sr1(pI2Cx, I2C_SR1_ADDR, start, Timeout);
	if (status != DRV_OK) {
		return master_abort(pI2CHandle, status);
	}

	/* 5. Clear ADDR with the mandatory read SR1 -> read SR2 sequence. Until this is
	 *    done the clock stays stretched and no data byte can be shifted out. */
	clear_ADDR_flag(pI2CHandle);

	/* 6. Push the payload out byte by byte. TXE=1 means DR is free for the next
	 *    byte while the previous one is still being shifted, which keeps the
	 *    transfer pipelined. AF here means the slave refused a data byte. */
	while (Len > 0U) {
		status = wait_sr1(pI2Cx, I2C_SR1_TXE, start, Timeout);
		if (status != DRV_OK) {
			return master_abort(pI2CHandle, status);
		}
		pI2Cx->DR = *pTxBuffer;
		pTxBuffer++;
		Len--;
	}

	/* 7. When the loop ends the last byte is written to DR but may still be on the
	 *    wire. Wait for TXE=1 and BTF=1 together: DR is empty and the shift
	 *    register is done, so a STOP now cannot truncate the final byte. */
	status = wait_sr1(pI2Cx, I2C_SR1_TXE, start, Timeout);
	if (status == DRV_OK) {
		status = wait_sr1(pI2Cx, I2C_SR1_BTF, start, Timeout);
	}
	if (status != DRV_OK) {
		return master_abort(pI2CHandle, status);
	}

	/* 8. Generate STOP and release the bus, unless the caller asked to hold it for
	 *    a repeated START (I2C_ENABLE_SR), which is what a register-read sequence
	 *    needs. Writing STOP also clears BTF. */
	if (Sr == I2C_DISABLE_SR) {
		pI2Cx->CR1 |= (1U << I2C_CR1_STOP);
	}

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - I2C_MasterReceiveData
 *
 * @brief              - Reads data from a slave device as bus master
 *
 * @param[in]          - pointer to the I2C Handle structure
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of bytes to receive
 * @param[in]          - 7-bit slave address
 * @param[in]          - I2C_ENABLE_SR or I2C_DISABLE_SR
 * @param[in]          - maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 *
 * @return             - DRV_OK, DRV_ERROR (slave answered NACK, or bad argument) or
 *                       DRV_TIMEOUT
 *
 * @Note               - blocking call. On DRV_ERROR and DRV_TIMEOUT a STOP is sent and ACK
 *                       is restored, so the bus is free again for the next transfer
 *
 ******************************************************************************************/
DRV_Status_t I2C_MasterReceiveData(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr, uint32_t Timeout) {
	I2C_RegDef_t *pI2Cx = pI2CHandle->Instance;
	DRV_Status_t status;

	if (pRxBuffer == NULL && Len > 0U) {
		return DRV_ERROR;
	}

	uint32_t start = SYSTICK_GetTick();

	/* 1. Generate the START condition. The peripheral first waits for the bus to be
	 *    free, then pulls SDA low while SCL is still high. */
	pI2Cx->CR1 |= (1U << I2C_CR1_START);

	/* 2. Wait until SB is set, which confirms the START was placed on the bus.
	 *    SB is cleared by reading SR1 and then writing DR (step 3). */
	status = wait_sr1(pI2Cx, I2C_SR1_SB, start, Timeout);
	if (status != DRV_OK) {
		return master_abort(pI2CHandle, status);
	}

	/* 3. Send the address phase: 7-bit slave address in bits [7:1] and the R/W bit
	 *    set in bit 0 to ask for a read. This DR write also clears SB. */
	pI2Cx->DR = (((uint32_t)SlaveAddr << 1) | 1U);

	/* 4. Wait for ADDR. A missing slave answers NACK: AF, DRV_ERROR at once.
	 *    SCL stays stretched low until ADDR is cleared, which is what gives us the
	 *    time to set up ACK/STOP below before any data byte is clocked in. */
	status = wait_sr1(pI2Cx, I2C_SR1_ADDR, start, Timeout);
	if (status != DRV_OK) {
		return master_abort(pI2CHandle, status);
	}

	if (Len == 0U) {
		/* 5a. Nothing to read: release the clock and close the transfer. */
		clear_ADDR_flag(pI2CHandle);
		if (Sr == I2C_DISABLE_SR) {
			pI2Cx->CR1 |= (1U << I2C_CR1_STOP);
		}
	} else if (Len == 1U) {
		/* 5b. Single byte. RM0390: ACK must be cleared BEFORE ADDR is cleared,
		 *     otherwise the hardware acknowledges the only byte and the slave
		 *     keeps driving the bus. STOP is programmed right after ADDR clears
		 *     and before the byte arrives. */
		I2C_ManageAcking(pI2Cx, I2C_ACK_DISABLE);
		clear_ADDR_flag(pI2CHandle);
		if (Sr == I2C_DISABLE_SR) {
			pI2Cx->CR1 |= (1U << I2C_CR1_STOP);
		}
		status = wait_sr1(pI2Cx, I2C_SR1_RXNE, start, Timeout);
		if (status != DRV_OK) {
			return master_abort(pI2CHandle, status);
		}
		*pRxBuffer = (uint8_t)pI2Cx->DR;
	} else {
		/* 5c. Multi byte. Read while RXNE=1 means a byte is ready in DR. When two
		 *     bytes are still outstanding, drop ACK and program STOP so the last
		 *     byte is answered with a NACK and the bus is released after it. */
		clear_ADDR_flag(pI2CHandle);

		for (uint32_t i = Len; i > 0U; i--) {
			status = wait_sr1(pI2Cx, I2C_SR1_RXNE, start, Timeout);
			if (status != DRV_OK) {
				return master_abort(pI2CHandle, status);
			}

			if (i == 2U) {
				I2C_ManageAcking(pI2Cx, I2C_ACK_DISABLE);
				if (Sr == I2C_DISABLE_SR) {
					pI2Cx->CR1 |= (1U << I2C_CR1_STOP);
				}
			}

			*pRxBuffer = (uint8_t)pI2Cx->DR;
			pRxBuffer++;
		}
	}

	/* 6. Restore ACK to whatever the handle was configured with, so the next
	 *    transfer does not inherit the NACK left over from this one. */
	if (pI2CHandle->Config.ACKControl == I2C_ACK_ENABLE) {
		I2C_ManageAcking(pI2Cx, I2C_ACK_ENABLE);
	}

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - I2C_PeripheralControl
 *
 * @brief              - Enables or disables the I2C peripheral (CR1 PE bit)
 *
 * @param[in]          - base address of the I2C peripheral
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - call this only after I2C_Init, because CCR, TRISE and FREQ can
 *                       only be programmed while PE is 0
 *
 ******************************************************************************************/
void I2C_PeripheralControl(I2C_RegDef_t *pI2Cx, DRV_State_t State) {
	if (State == DRV_ENABLE) {
		pI2Cx->CR1 |= (1U << I2C_CR1_PE);
	} else {
		pI2Cx->CR1 &= ~(1U << I2C_CR1_PE);
	}
}

/******************************************************************************************
 * @fn                 - I2C_IsBusy
 *
 * @brief              - Reports whether a transfer is in progress on the bus
 *
 * @param[in]          - base address of the I2C peripheral
 *
 * @return             - non-zero when the bus is busy, 0 when it is free
 *
 * @Note               - BUSY is set on a START and cleared on a STOP, by any master on the
 *                       bus, not only this one
 *
 ******************************************************************************************/
int I2C_IsBusy(I2C_RegDef_t *pI2Cx) {
	return (pI2Cx->SR2 & (1U << I2C_SR2_BUSY)) ? 1 : 0;
}

/******************************************************************************************
 * @fn                 - I2C_ManageAcking
 *
 * @brief              - Enables or disables automatic ACK after each received byte
 *
 * @param[in]          - base address of the I2C peripheral
 * @param[in]          - I2C_ACK_ENABLE or I2C_ACK_DISABLE
 *
 * @return             - none
 *
 * @Note               - a slave only answers its address while ACK is 1. RM0390: hardware
 *                       clears ACK while PE is 0, so call this after PE is set
 *
 ******************************************************************************************/
void I2C_ManageAcking(I2C_RegDef_t *pI2Cx, uint8_t AckControl) {
    // ACK lives in CR1 bit 10. SR1 is status only and must never be used here.
    if (AckControl == I2C_ACK_ENABLE) {
        pI2Cx->CR1 |= (1U << I2C_CR1_ACK);
    } else {
        pI2Cx->CR1 &= ~(1U << I2C_CR1_ACK);
    }
}

/******************************************************************************************
 * @fn                 - I2C_MasterSendDataIT
 *
 * @brief              - Starts an interrupt driven transmission to a slave device
 *
 * @param[in]          - pointer to the I2C Handle structure
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of bytes to send
 * @param[in]          - 7-bit slave address
 * @param[in]          - I2C_ENABLE_SR or I2C_DISABLE_SR
 *
 * @return             - DRV_OK when started, DRV_BUSY when a transfer is still running on
 *                       this handle (nothing changed)
 *
 * @Note               - returns as soon as START is requested. The rest of the transfer runs
 *                       in I2C_EV_IRQHandling. The buffer must stay valid until the
 *                       I2C_EVENT_TX_CMPLT callback arrives
 *
 ******************************************************************************************/
DRV_Status_t I2C_MasterSendDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	uint8_t busystate = pI2CHandle->TxRxState;

	if ((busystate == I2C_BUSY_IN_TX) || (busystate == I2C_BUSY_IN_RX)) {
		return DRV_BUSY;
	}

	pI2CHandle->pTxBuffer = pTxBuffer;
	pI2CHandle->TxLen = Len;
	pI2CHandle->TxRxState = I2C_BUSY_IN_TX;
	pI2CHandle->DevAddr = SlaveAddr;
	pI2CHandle->Sr = Sr;

	/* START makes SB fire, which is where the address phase is driven from. */
	pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_START);

	/* Arm the three interrupt sources the state machine runs on. */
	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITEVTEN);
	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITBUFEN);
	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITERREN);

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - I2C_MasterReceiveDataIT
 *
 * @brief              - Starts an interrupt driven reception from a slave device
 *
 * @param[in]          - pointer to the I2C Handle structure
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of bytes to receive
 * @param[in]          - 7-bit slave address
 * @param[in]          - I2C_ENABLE_SR or I2C_DISABLE_SR
 *
 * @return             - DRV_OK when started, DRV_BUSY when a transfer is still running on
 *                       this handle (nothing changed)
 *
 * @Note               - the buffer must stay valid until the I2C_EVENT_RX_CMPLT callback
 *
 ******************************************************************************************/
DRV_Status_t I2C_MasterReceiveDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	uint8_t busystate = pI2CHandle->TxRxState;

	if ((busystate == I2C_BUSY_IN_TX) || (busystate == I2C_BUSY_IN_RX)) {
		return DRV_BUSY;
	}

	pI2CHandle->pRxBuffer = pRxBuffer;
	pI2CHandle->RxLen = Len;
	/* RxSize is the total length. RxLen counts down; the RXNE handler needs
	 * both to know when only two bytes are left and ACK must be dropped. */
	pI2CHandle->RxSize = Len;
	pI2CHandle->TxRxState = I2C_BUSY_IN_RX;
	pI2CHandle->DevAddr = SlaveAddr;
	pI2CHandle->Sr = Sr;

	pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_START);

	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITEVTEN);
	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITBUFEN);
	pI2CHandle->Instance->CR2 |= (1U << I2C_CR2_ITERREN);

	return DRV_OK;
}

/******************************************************************************************
 * @fn                 - I2C_SlaveSendData
 *
 * @brief              - Places one byte in the data register while addressed as a slave
 *
 * @param[in]          - base address of the I2C peripheral
 * @param[in]          - byte to send
 *
 * @return             - none
 *
 * @Note               - call this from the I2C_EVENT_DATA_REQ callback, where TXE is set
 *                       and the master is waiting for a byte
 *
 ******************************************************************************************/
void I2C_SlaveSendData(I2C_RegDef_t *pI2Cx, uint8_t data) {
	pI2Cx->DR = data;
}

/******************************************************************************************
 * @fn                 - I2C_SlaveReceiveData
 *
 * @brief              - Reads one byte from the data register while addressed as a slave
 *
 * @param[in]          - base address of the I2C peripheral
 *
 * @return             - the received byte
 *
 * @Note               - call this from the I2C_EVENT_DATA_RCV callback, where RXNE is set
 *
 ******************************************************************************************/
uint8_t I2C_SlaveReceiveData(I2C_RegDef_t *pI2Cx) {
	return (uint8_t)pI2Cx->DR;
}

/******************************************************************************************
 * @fn                 - I2C_IRQInterruptConfig
 *
 * @brief              - Enables or disables the I2C interrupt line in the NVIC
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - DRV_ENABLE or DRV_DISABLE
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQInterruptConfig, kept so the I2C API is complete
 *
 ******************************************************************************************/
void I2C_IRQInterruptConfig(uint8_t IRQNumber, DRV_State_t State) {
	NVIC_IRQInterruptConfig(IRQNumber, State);
}

/******************************************************************************************
 * @fn                 - I2C_IRQPriorityConfig
 *
 * @brief              - Sets the priority of the I2C interrupt line
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - priority 0 (most urgent) .. 15 (least urgent)
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQPriorityConfig, kept so the I2C API is complete
 *
 ******************************************************************************************/
void I2C_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	NVIC_IRQPriorityConfig(IRQNumber, IRQPriority);
}

/******************************************************************************************
 * @fn                 - I2C_EV_IRQHandling
 *
 * @brief              - Handles the I2C event interrupts (SB, ADDR, BTF, STOPF, TXE, RXNE)
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - should be called inside the I2Cx_EV_IRQHandler vector
 *
 ******************************************************************************************/
void I2C_EV_IRQHandling(I2C_Handle_t *pI2CHandle) {
	uint32_t itevten;
	uint32_t itbufen;
	uint32_t sr1;

	itevten = pI2CHandle->Instance->CR2 & (1U << I2C_CR2_ITEVTEN);
	itbufen = pI2CHandle->Instance->CR2 & (1U << I2C_CR2_ITBUFEN);

	/* Every event below is gated by ITEVTEN, so bail out early if it is off. */
	if (!itevten) {
		return;
	}

	sr1 = pI2CHandle->Instance->SR1;

	/* SB: master mode only, set right after START. Cleared by reading SR1 (done
	 * above) and then writing DR, which is the address phase. */
	if (sr1 & (1U << I2C_SR1_SB)) {
		if (pI2CHandle->TxRxState == I2C_BUSY_IN_TX) {
			pI2CHandle->Instance->DR = (((uint32_t)pI2CHandle->DevAddr << 1) & ~(1U));
		} else if (pI2CHandle->TxRxState == I2C_BUSY_IN_RX) {
			pI2CHandle->Instance->DR = (((uint32_t)pI2CHandle->DevAddr << 1) | 1U);
		}
	}

	/* ADDR: address sent (master) or matched (slave). The clock is stretched until
	 * it is cleared, which is the only window where ACK can still be set up for a
	 * single byte reception. */
	if (sr1 & (1U << I2C_SR1_ADDR)) {
		uint32_t sr2 = pI2CHandle->Instance->SR2;

		if ((sr2 & (1U << I2C_SR2_MSL)) &&
		    (pI2CHandle->TxRxState == I2C_BUSY_IN_RX) &&
		    (pI2CHandle->RxSize == 1U)) {
			/* Only one byte coming: NACK it before releasing the clock. */
			I2C_ManageAcking(pI2CHandle->Instance, I2C_ACK_DISABLE);
		}

		/* Reading SR1 above plus SR2 here already cleared ADDR. */
		(void)sr2;
	}

	/* BTF: shift register and DR are both done. For a transmitter this is the safe
	 * point to place STOP without truncating the last byte. */
	if (sr1 & (1U << I2C_SR1_BTF)) {
		if (pI2CHandle->TxRxState == I2C_BUSY_IN_TX) {
			if (sr1 & (1U << I2C_SR1_TXE)) {
				if (pI2CHandle->TxLen == 0U) {
					if (pI2CHandle->Sr == I2C_DISABLE_SR) {
						pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_STOP);
					}

					I2C_CloseSendData(pI2CHandle);
					I2C_ApplicationEventCallback(pI2CHandle, I2C_EVENT_TX_CMPLT);
				}
			}
		}
		/* Receiver BTF needs no action here: the RXNE path already handles the
		 * ACK and STOP sequencing for the last two bytes. */
	}

	/* STOPF: slave mode only, set when the master released the bus. Cleared by
	 * reading SR1 (done above) and then writing CR1. */
	if (sr1 & (1U << I2C_SR1_STOPF)) {
		pI2CHandle->Instance->CR1 |= 0x0000U;
		I2C_ApplicationEventCallback(pI2CHandle, I2C_EVENT_STOP);
	}

	/* TXE: DR is free for another byte. */
	if (itbufen && (sr1 & (1U << I2C_SR1_TXE))) {
		if (pI2CHandle->Instance->SR2 & (1U << I2C_SR2_MSL)) {
			if (pI2CHandle->TxRxState == I2C_BUSY_IN_TX) {
				I2C_MasterHandleTXEInterrupt(pI2CHandle);
			}
		} else {
			/* Slave: only meaningful while addressed as a transmitter. */
			if (pI2CHandle->Instance->SR2 & (1U << I2C_SR2_TRA)) {
				I2C_ApplicationEventCallback(pI2CHandle, I2C_EVENT_DATA_REQ);
			}
		}
	}

	/* RXNE: a byte is waiting in DR. */
	if (itbufen && (sr1 & (1U << I2C_SR1_RXNE))) {
		if (pI2CHandle->Instance->SR2 & (1U << I2C_SR2_MSL)) {
			if (pI2CHandle->TxRxState == I2C_BUSY_IN_RX) {
				I2C_MasterHandleRXNEInterrupt(pI2CHandle);
			}
		} else {
			if (!(pI2CHandle->Instance->SR2 & (1U << I2C_SR2_TRA))) {
				I2C_ApplicationEventCallback(pI2CHandle, I2C_EVENT_DATA_RCV);
			}
		}
	}
}

/******************************************************************************************
 * @fn                 - I2C_ER_IRQHandling
 *
 * @brief              - Handles the I2C error interrupts (BERR, ARLO, AF, OVR, TIMEOUT)
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - should be called inside the I2Cx_ER_IRQHandler vector. Every error
 *                       flag is rc_w0, so it is cleared by writing 0 to that bit only
 *
 ******************************************************************************************/
void I2C_ER_IRQHandling(I2C_Handle_t *pI2CHandle) {
	uint32_t iterren;
	uint32_t sr1;

	iterren = pI2CHandle->Instance->CR2 & (1U << I2C_CR2_ITERREN);
	if (!iterren) {
		return;
	}

	sr1 = pI2CHandle->Instance->SR1;

	/* BERR: a START or STOP appeared at an invalid position on the bus. */
	if (sr1 & (1U << I2C_SR1_BERR)) {
		pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_BERR);
		I2C_ApplicationEventCallback(pI2CHandle, I2C_ERROR_BERR);
	}

	/* ARLO: lost arbitration against another master. The peripheral has already
	 * dropped back to slave mode by itself. */
	if (sr1 & (1U << I2C_SR1_ARLO)) {
		pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_ARLO);
		I2C_ApplicationEventCallback(pI2CHandle, I2C_ERROR_ARLO);
	}

	/* AF: no ACK where one was expected. As a master this usually means the slave
	 * is absent, so close the transfer and release the bus. */
	if (sr1 & (1U << I2C_SR1_AF)) {
		pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_AF);

		if (pI2CHandle->Instance->SR2 & (1U << I2C_SR2_MSL)) {
			pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_STOP);

			if (pI2CHandle->TxRxState == I2C_BUSY_IN_TX) {
				I2C_CloseSendData(pI2CHandle);
			} else if (pI2CHandle->TxRxState == I2C_BUSY_IN_RX) {
				I2C_CloseReceiveData(pI2CHandle);
			}
		}

		I2C_ApplicationEventCallback(pI2CHandle, I2C_ERROR_AF);
	}

	/* OVR: a byte was lost because software did not read or write DR in time. */
	if (sr1 & (1U << I2C_SR1_OVR)) {
		pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_OVR);
		I2C_ApplicationEventCallback(pI2CHandle, I2C_ERROR_OVR);
	}

	/* TIMEOUT: SCL was held low past the SMBus limit. */
	if (sr1 & (1U << I2C_SR1_TIMEOUT)) {
		pI2CHandle->Instance->SR1 &= ~(1U << I2C_SR1_TIMEOUT);
		I2C_ApplicationEventCallback(pI2CHandle, I2C_ERROR_TIMEOUT);
	}
}

/******************************************************************************************
 * @fn                 - I2C_MasterHandleTXEInterrupt
 *
 * @brief              - Feeds the next byte to DR while acting as master transmitter
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - the transfer is closed from the BTF branch, not here, so that the
 *                       last byte is fully on the wire before STOP
 *
 ******************************************************************************************/
static void I2C_MasterHandleTXEInterrupt(I2C_Handle_t *pI2CHandle) {
	if (pI2CHandle->TxLen > 0U) {
		pI2CHandle->Instance->DR = *(pI2CHandle->pTxBuffer);
		pI2CHandle->pTxBuffer++;
		pI2CHandle->TxLen--;
	}
}

/******************************************************************************************
 * @fn                 - I2C_MasterHandleRXNEInterrupt
 *
 * @brief              - Drains one byte from DR while acting as master receiver
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - ACK is dropped and STOP is programmed while two bytes are still
 *                       outstanding, so the final byte is answered with a NACK
 *
 ******************************************************************************************/
static void I2C_MasterHandleRXNEInterrupt(I2C_Handle_t *pI2CHandle) {
	if (pI2CHandle->RxSize == 1U) {
		/* ACK was already cleared in the ADDR branch for this case. */
		*(pI2CHandle->pRxBuffer) = (uint8_t)pI2CHandle->Instance->DR;
		pI2CHandle->RxLen--;
	} else if (pI2CHandle->RxSize > 1U) {
		if (pI2CHandle->RxLen == 2U) {
			I2C_ManageAcking(pI2CHandle->Instance, I2C_ACK_DISABLE);
		}

		*(pI2CHandle->pRxBuffer) = (uint8_t)pI2CHandle->Instance->DR;
		pI2CHandle->pRxBuffer++;
		pI2CHandle->RxLen--;
	}

	if (pI2CHandle->RxLen == 0U) {
		if (pI2CHandle->Sr == I2C_DISABLE_SR) {
			pI2CHandle->Instance->CR1 |= (1U << I2C_CR1_STOP);
		}

		I2C_CloseReceiveData(pI2CHandle);
		I2C_ApplicationEventCallback(pI2CHandle, I2C_EVENT_RX_CMPLT);
	}
}

/******************************************************************************************
 * @fn                 - I2C_CloseSendData
 *
 * @brief              - Ends an interrupt driven transmission and frees the handle
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - does not generate STOP; the caller decides that based on Sr
 *
 ******************************************************************************************/
static void I2C_CloseSendData(I2C_Handle_t *pI2CHandle) {
	pI2CHandle->Instance->CR2 &= ~(1U << I2C_CR2_ITBUFEN);
	pI2CHandle->Instance->CR2 &= ~(1U << I2C_CR2_ITEVTEN);

	pI2CHandle->TxRxState = I2C_READY;
	pI2CHandle->pTxBuffer = NULL;
	pI2CHandle->TxLen = 0U;
}

/******************************************************************************************
 * @fn                 - I2C_CloseReceiveData
 *
 * @brief              - Ends an interrupt driven reception and frees the handle
 *
 * @param[in]          - pointer to the active I2C Handle structure
 *
 * @return             - none
 *
 * @Note               - restores ACK to the configured value so the next transfer does not
 *                       inherit the NACK used to end this one
 *
 ******************************************************************************************/
static void I2C_CloseReceiveData(I2C_Handle_t *pI2CHandle) {
	pI2CHandle->Instance->CR2 &= ~(1U << I2C_CR2_ITBUFEN);
	pI2CHandle->Instance->CR2 &= ~(1U << I2C_CR2_ITEVTEN);

	pI2CHandle->TxRxState = I2C_READY;
	pI2CHandle->pRxBuffer = NULL;
	pI2CHandle->RxLen = 0U;
	pI2CHandle->RxSize = 0U;

	if (pI2CHandle->Config.ACKControl == I2C_ACK_ENABLE) {
		I2C_ManageAcking(pI2CHandle->Instance, I2C_ACK_ENABLE);
	}
}

__attribute__((weak)) void I2C_ApplicationEventCallback(I2C_Handle_t *pI2CHandle, uint8_t AppEv)
{
    // This is an empty placeholder.
    // The user can override this function in main.c without modifying the driver!
    (void)pI2CHandle;
    (void)AppEv;
}
