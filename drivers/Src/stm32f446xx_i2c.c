/*
 * stm32f446xx_i2c.c
 *
 *  Created on: 13 Sept 2026
 *  Author: vitaliiantoniuk
 */

#include <stddef.h>
#include <stdint.h>
#include "stm32f446xx.h"

/******************************************************************************************
 * @fn                 - I2C_PeriClockControl
 *
 * @brief              - This function enables or disables peripheral clock for the given I2Cx
 *
 * @param[in]          - base address of the i2c peripheral
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - not implemented
 *
 ******************************************************************************************/
void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi) {
    if (EnorDi == ENABLE) {
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
 * @Note               - not implemented
 *
 ******************************************************************************************/
void I2C_Init(I2C_Handle_t *pI2CHandle) {
    // Must be at least 16-bit: CCR packs FS at bit 15 and DUTY at bit 14, and
    // OAR1 needs bit 14 held high. Use 32-bit to match the register width.
    uint32_t tempreg = 0;

    // config CR1
    tempreg |= pI2CHandle->I2C_Config.I2C_ACKControl << I2C_CR1_ACK;
    pI2CHandle->pI2Cx->CR1 = tempreg;
    
	// configure PCLK
	tempreg = 0;
	tempreg |= RCC_GetPCLK1Value() / 1000000U;
	pI2CHandle->pI2Cx->CR2 = (tempreg & 0x3F); // FREQ is [5:0]

	// configure own address
	tempreg = 0;
	tempreg |= (uint32_t)(pI2CHandle->I2C_Config.I2C_DeviceAddress & 0x7FU) << I2C_OAR1_ADD71;
	tempreg |= (1U << 14); // RM0390: bit 14 is reserved but must be kept at 1
	pI2CHandle->pI2Cx->OAR1 = tempreg;

	// configure CCR
	tempreg = 0;
	uint32_t ccr_value = 0;
	if (pI2CHandle->I2C_Config.I2C_SCLSpeed == I2C_SCL_SPEED_SM) {
	    // Standard mode: Thigh = Tlow = CCR * TPCLK1, so one SCL period is 2*CCR
	    ccr_value = RCC_GetPCLK1Value() / (2U * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		tempreg |= ((ccr_value << I2C_CCR_CCR) & 0xFFF);
	} else if (
	    pI2CHandle->I2C_Config.I2C_SCLSpeed == I2C_SCL_SPEED_FM2K ||
		pI2CHandle->I2C_Config.I2C_SCLSpeed == I2C_SCL_SPEED_FM4K			
	) {
	    tempreg |= (1U << I2C_CCR_FS);
		tempreg |= ((uint32_t)pI2CHandle->I2C_Config.I2C_FMDutyCycle << I2C_CCR_DUTY);
		if (pI2CHandle->I2C_Config.I2C_FMDutyCycle == I2C_FM_DUTY_2) {
		    // Tlow/Thigh = 2, so one SCL period spans 3*CCR
            ccr_value = RCC_GetPCLK1Value() / (3U * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		} else {
		    // Tlow/Thigh = 16/9, so one SCL period spans 25*CCR
		    ccr_value = RCC_GetPCLK1Value() / (25U * pI2CHandle->I2C_Config.I2C_SCLSpeed);
		}
        tempreg |= ((ccr_value << I2C_CCR_CCR) & 0xFFF);
	}
	pI2CHandle->pI2Cx->CCR = tempreg;
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
 * @Note               - not implemented
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
 *
 * @return             - none
 *
 * @Note               - this is a blocking call, not implemented
 *
 ******************************************************************************************/
void I2C_MasterSendData(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	(void)pI2CHandle;
	(void)pTxBuffer;
	(void)Len;
	(void)SlaveAddr;
	(void)Sr;
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
 *
 * @return             - none
 *
 * @Note               - this is a blocking call, not implemented
 *
 ******************************************************************************************/
void I2C_MasterReceiveData(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	(void)pI2CHandle;
	(void)pRxBuffer;
	(void)Len;
	(void)SlaveAddr;
	(void)Sr;
}

void I2C_PeripheralControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi) {
	(void)pI2Cx;
	(void)EnorDi;
}

int I2C_IsBusy(I2C_RegDef_t *pI2Cx) {
	(void)pI2Cx;
	return 0;
}

void I2C_ManageAcking(I2C_RegDef_t *pI2Cx, uint8_t EnorDi) {
	(void)pI2Cx;
	(void)EnorDi;
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
 *
 * @return             - none
 *
 * @Note               - this is a blocking call, not implemented
 *
 ******************************************************************************************/
void I2C_MasterReceiveData(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	(void)pI2CHandle;
	(void)pRxBuffer;
	(void)Len;
	(void)SlaveAddr;
	(void)Sr;
}

uint8_t I2C_MasterSendDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	(void)pI2CHandle;
	(void)pTxBuffer;
	(void)Len;
	(void)SlaveAddr;
	(void)Sr;
	return I2C_READY;
}

uint8_t I2C_MasterReceiveDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr) {
	(void)pI2CHandle;
	(void)pRxBuffer;
	(void)Len;
	(void)SlaveAddr;
	(void)Sr;
	return I2C_READY;
}

void I2C_SlaveSendData(I2C_RegDef_t *pI2Cx, uint8_t data) {
	(void)pI2Cx;
	(void)data;
}

uint8_t I2C_SlaveReceiveData(I2C_RegDef_t *pI2Cx) {
	(void)pI2Cx;
	return 0;
}

/******************************************************************************************
 * @fn                 - I2C_IRQInterruptConfig
 *
 * @brief              - Enables or disables the interrupt processing for a given IRQ number in NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - not implemented
 *
 ******************************************************************************************/
void I2C_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi) {
	(void)IRQNumber;
	(void)EnorDi;
}

/******************************************************************************************
 * @fn                 - I2C_IRQPriorityConfig
 *
 * @brief              - Configures the priority level of a given IRQ number in the NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - priority level value
 *
 * @return             - none
 *
 * @Note               - not implemented
 *
 ******************************************************************************************/
void I2C_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	(void)IRQNumber;
	(void)IRQPriority;
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
 * @Note               - should be called inside the I2Cx_EV_IRQHandler vector, not implemented
 *
 ******************************************************************************************/
void I2C_EV_IRQHandling(I2C_Handle_t *pI2CHandle) {
	(void)pI2CHandle;
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
 * @Note               - should be called inside the I2Cx_ER_IRQHandler vector, not implemented
 *
 ******************************************************************************************/
void I2C_ER_IRQHandling(I2C_Handle_t *pI2CHandle) {
	(void)pI2CHandle;
}

__attribute__((weak)) void I2C_ApplicationEventCallback(I2C_Handle_t *pI2CHandle, uint8_t AppEv)
{
    // This is an empty placeholder.
    // The user can override this function in main.c without modifying the driver!
}
