/*
 * stm32f446xx_gpio.c
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 */

#include "stm32f446xx_gpio.h"

/******************************************************************************************
 * @fn                 - GPIO_PeriClockControl
 *
 * @brief              - This function enables or disables peripheral clock for the given GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnorDi){
	if (EnorDi == ENABLE) {
		if (pGPIOx == GPIOA) {
			GPIOA_PCLK_EN();
		} else if (pGPIOx == GPIOB) {
			GPIOB_PCLK_EN();
		} else if (pGPIOx == GPIOC) {
			GPIOC_PCLK_EN();
		} else if (pGPIOx == GPIOD) {
			GPIOD_PCLK_EN();
		} else if (pGPIOx == GPIOE) {
			GPIOE_PCLK_EN();
		} else if (pGPIOx == GPIOF) {
			GPIOF_PCLK_EN();
		} else if (pGPIOx == GPIOG) {
			GPIOG_PCLK_EN();
		} else if (pGPIOx == GPIOH) {
			GPIOH_PCLK_EN();
		}
	}else {
		if (pGPIOx == GPIOA) {
			GPIOA_PCLK_DI();
		} else if (pGPIOx == GPIOB) {
			GPIOB_PCLK_DI();
		} else if (pGPIOx == GPIOC) {
			GPIOC_PCLK_DI();
		} else if (pGPIOx == GPIOD) {
			GPIOD_PCLK_DI();
		} else if (pGPIOx == GPIOE) {
			GPIOE_PCLK_DI();
		} else if (pGPIOx == GPIOF) {
			GPIOF_PCLK_DI();
		} else if (pGPIOx == GPIOG) {
			GPIOG_PCLK_DI();
		} else if (pGPIOx == GPIOH) {
			GPIOH_PCLK_DI();
		}
	}
}

/******************************************************************************************
 * @fn                 - GPIO_Init
 *
 * @brief              - Initializes the given GPIO port and pin with specified configurations
 *
 * @param[in]          - pointer to the GPIO Handle structure containing base address and pin config
 *
 * @return             - none
 *
 * @Note               - interrupt modes (GPIO_MODE_IT_*) also configure the pin as input,
 *                       route the pin to its EXTI line and unmask the line. The NVIC
 *                       line still has to be enabled with GPIO_IRQInterruptConfig
 *
 ******************************************************************************************/
void GPIO_Init(GPIO_Handle_t *pGPIOHandle){
	uint32_t temp = 0;
	uint8_t pin_num = pGPIOHandle->Config.PinNumber;

	// 1. Configure the Mode of the GPIO pin
	if(pGPIOHandle->Config.Mode <= GPIO_MODE_ANALOG)
	{
		// Non-interrupt modes: clear the 2 bits first, then set the mode value
		temp = (pGPIOHandle->Config.Mode << (2 * pin_num));
		pGPIOHandle->Instance->MODER &= ~(0x3 << (2 * pin_num));
		pGPIOHandle->Instance->MODER |= temp;
	}
	else
	{
		// Interrupt modes: the pin must be an input (MODER = 00). Without this a
		// pin that was an output before stays an output, and the EXTI line would
		// only ever see the level we drive ourselves.
		pGPIOHandle->Instance->MODER &= ~(0x3 << (2 * pin_num));

		if (pGPIOHandle->Config.Mode == GPIO_MODE_IT_FT) {
			EXTI->FTSR |= (1U << pin_num);
			EXTI->RTSR &= ~(1U << pin_num);
		} else if (pGPIOHandle->Config.Mode == GPIO_MODE_IT_RT) {
			EXTI->RTSR |= (1U << pin_num);
			EXTI->FTSR &= ~(1U << pin_num);
		} else if(pGPIOHandle->Config.Mode == GPIO_MODE_IT_RFT) {
			EXTI->FTSR |= (1U << pin_num);
			EXTI->RTSR |= (1U << pin_num);
		}

		// configure system configuration controller
		temp = pin_num / 4;
		uint8_t pos = pin_num % 4;
		uint8_t portcode = 0;
		if (pGPIOHandle->Instance == GPIOA) {
			portcode = 0x0;
		} else if (pGPIOHandle->Instance == GPIOB) {
			portcode = 0x1;
		} else if (pGPIOHandle->Instance == GPIOC) {
			portcode = 0x2;
		} else if (pGPIOHandle->Instance == GPIOD) {
			portcode = 0x3;
		} else if (pGPIOHandle->Instance == GPIOE) {
			portcode = 0x4;
		} else if (pGPIOHandle->Instance == GPIOF) {
			portcode = 0x5;
		} else if (pGPIOHandle->Instance == GPIOG) {
			portcode = 0x6;
		} else if (pGPIOHandle->Instance == GPIOH) {
			portcode = 0x7;
		}
		SYSCFG_PCLK_EN();
		SYSCFG->EXTICR[temp] &= ~(0xF << (pos * 4));
		SYSCFG->EXTICR[temp] |= (portcode << (pos * 4));

		// set external interrupt/event line mapping
		EXTI->IMR |= (1U << pin_num);
	}

	// 2. Configure the Speed
	temp = (pGPIOHandle->Config.Speed << (2 * pin_num));
	pGPIOHandle->Instance->OSPEEDR &= ~(0x3 << (2 * pin_num));
	pGPIOHandle->Instance->OSPEEDR |= temp;

	// 3. Configure the Pull-up/Pull-down settings
	temp = (pGPIOHandle->Config.PuPdControl << (2 * pin_num));
	pGPIOHandle->Instance->PUPDR &= ~(0x3 << (2 * pin_num));
	pGPIOHandle->Instance->PUPDR |= temp;

	// 4. Configure the Output Type (Push-Pull or Open-Drain)
	temp = (pGPIOHandle->Config.OPType << pin_num);
	pGPIOHandle->Instance->OTYPER &= ~(0x1 << pin_num);
	pGPIOHandle->Instance->OTYPER |= temp;

	// 5. Configure the Alternate Functionality
	if(pGPIOHandle->Config.Mode == GPIO_MODE_ALTFN)
	{
		temp = pin_num / 8;
		uint8_t pos = pin_num % 8;
		pGPIOHandle->Instance->AFR[temp] &= ~(0xF << (4 * pos));
		pGPIOHandle->Instance->AFR[temp] |= (pGPIOHandle->Config.AltFunMode << (4 * pos));
	}
}

/******************************************************************************************
 * @fn                 - GPIO_DeInit
 *
 * @brief              - Resets all registers of a given GPIO port back to their reset values
 *
 * @param[in]          - base address of the gpio peripheral
 *
 * @return             - none
 *
 * @Note               - Uses the RCC peripheral reset registers
 *
 ******************************************************************************************/
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx){
	if (pGPIOx == GPIOA) {
		GPIOA_REG_RESET();
	} else if (pGPIOx == GPIOB) {
		GPIOB_REG_RESET();
	} else if (pGPIOx == GPIOC) {
		GPIOC_REG_RESET();
	} else if (pGPIOx == GPIOD) {
		GPIOD_REG_RESET();
	} else if (pGPIOx == GPIOE) {
		GPIOE_REG_RESET();
	} else if (pGPIOx == GPIOF) {
		GPIOF_REG_RESET();
	} else if (pGPIOx == GPIOG) {
		GPIOG_REG_RESET();
	} else if (pGPIOx == GPIOH) {
		GPIOH_REG_RESET();
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
 * @Note               - none
 *
 ******************************************************************************************/
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber) {
	uint8_t value;
	value = (uint8_t )((pGPIOx->IDR >> PinNumber) & 0x00000001);

	return value;
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
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx){
	uint16_t value;
	value = (uint16_t) pGPIOx->IDR;

	return value;
}

/******************************************************************************************
 * @fn                 - GPIO_WriteToOutputPin
 *
 * @brief              - Writes a digital value (HIGH or LOW) to a specific pin on a GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 * @param[in]          - pin number to write to (0 to 15)
 * @param[in]          - output value (GPIO_PIN_SET or GPIO_PIN_RESET)
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value){
	if (Value == GPIO_PIN_SET) {
		pGPIOx->ODR |= (1 << PinNumber);
	}else {
		pGPIOx->ODR &= ~(1 << PinNumber);
	}
}

/******************************************************************************************
 * @fn                 - GPIO_WriteToOutputPort
 *
 * @brief              - Writes a complete 16-bit value to an entire GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 * @param[in]          - 16-bit value to write to the port output register
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value){
	pGPIOx->ODR = Value;
}

/******************************************************************************************
 * @fn                 - GPIO_ToggleOutputPin
 *
 * @brief              - Toggles the current output state of a specific pin on a GPIO port
 *
 * @param[in]          - base address of the gpio peripheral
 * @param[in]          - pin number to toggle (0 to 15)
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
	pGPIOx->ODR ^= (1 << PinNumber);
}

/******************************************************************************************
 * @fn                 - GPIO_IRQInterruptConfig
 *
 * @brief              - Enables or disables the GPIO interrupt line in the NVIC
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQInterruptConfig, kept so the GPIO API is complete
 *
 ******************************************************************************************/
void GPIO_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi) {
	NVIC_IRQInterruptConfig(IRQNumber, EnorDi);
}

/******************************************************************************************
 * @fn                 - GPIO_IRQPriorityConfig
 *
 * @brief              - Sets the priority of the GPIO interrupt line
 *
 * @param[in]          - IRQ number, one of the IRQ_NO_* macros
 * @param[in]          - priority 0 (most urgent) .. 15 (least urgent)
 *
 * @return             - none
 *
 * @Note               - same as NVIC_IRQPriorityConfig, kept so the GPIO API is complete
 *
 ******************************************************************************************/
void GPIO_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	NVIC_IRQPriorityConfig(IRQNumber, IRQPriority);
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
void GPIO_IRQHandling(uint8_t PinNumber)
{
    // Validate if the specific line is pending
    if (EXTI->PR & (1U << PinNumber))
    {
        // Clear the pending latch bit by writing a 1 to it
        EXTI->PR = (1U << PinNumber);
    }
}
