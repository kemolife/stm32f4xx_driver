/*
 * stm32f446xx_gpio.h
 *
 *  Created on: 5 Sept 2026
 *      Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_GPIO_H_
#define INC_STM32F446XX_GPIO_H_

#include "stm32f446xx.h"

/**
 * @brief GPIO Pin Configuration structure definition
 */
typedef struct {
    uint8_t GPIO_PinNumber;       /* Pin number selection (0 to 15)                 */
    uint8_t GPIO_PinMode;         /*!< possible value from @GPIO_PIN_MODES >        */
    uint8_t GPIO_PinSpeed;        /*!< possible value from @GPIO_PIN_OUTPUT_SPEED > */
    uint8_t GPIO_PinPuPdControl;  /*!< possible value from @GPIO_PIN_PUPD > */
    uint8_t GPIO_PinOPType;       /*!< possible value from @GPIO_PIN_OUTPUT_PYPES > */
    uint8_t GPIO_PinAltFunMode;   /* Alternate function selection (AF0 to AF15)     */
} GPIO_PinConfig_t;

typedef struct {
	GPIO_RegDef_t *pGPIOx;
	GPIO_PinConfig_t GPIO_PinConfig;
} GPIO_Handle_t;

/*
 * @GPIO_PIN_MODES
 * GPIO pin possible modes
 */
#define GPIO_MODE_IN      0
#define GPIO_MODE_OUT     1
#define GPIO_MODE_ALTFN   2
#define GPIO_MODE_ANALOG  3
#define GPIO_MODE_IT_FT   4
#define GPIO_MODE_IT_RT   5
#define GPIO_MODE_IT_RFT  6

/*
 * @GPIO_PIN_OUTPUT_PYPES
 * GPIO pin possible output types
 */
#define GPIO_OP_TYPE_PP   0
#define GPIO_OP_TYPE_OD   1

/*
 * @GPIO_PIN_OUTPUT_SPEED
 * GPIO pin possible output speeds
 */
#define GPIO_SPEED_LOW      0
#define GPIO_SPEED_MEDIUM   1
#define GPIO_SPEED_FAST     2
#define GPIO_SPEED_HIGH    	3

/*
 * @GPIO_PIN_PUPD
 * GPIO pin possible pull-up/pull-down
 */
#define GPIO_NOT_PUPD     0
#define GPIO_PIN_PU       1
#define GPIO_PIN_PD       2

#define GPIO_PIN_SET      1
#define GPIO_PIN_RESET    0


/* ========================================================================== */
/*                       DRIVER API FOR GPIO PERIPHERAL                       */
/* ========================================================================== */

/*
 * Peripheral Clock setup
 */
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnorDi);

/*
 * Init and De-init
 */
void GPIO_Init(GPIO_Handle_t *pGPIOHandle);
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx);

/*
 * Data read and write
 */
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx);
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value);
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value);
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);

/*
 * IRQ Configuration and ISR handling
 */
void GPIO_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi);
void GPIO_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);
void GPIO_IRQHandling(uint8_t PinNumber);

#endif /* INC_STM32F446XX_GPIO_H_ */

