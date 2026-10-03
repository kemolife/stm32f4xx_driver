/*
 * stm32f446xx_spi.h
 *
 *  Created on: 5 Sept 2026
 *  Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_SPI_H_
#define INC_STM32F446XX_SPI_H_

#include "stm32f446xx.h"

/**
 * @brief SPI Peripheral Configuration structure definition
 */
typedef struct {
    uint8_t DeviceMode;           /* Master or Slave configuration (@SPI_DeviceMode)     */
    uint8_t BusConfig;            /* Full-duplex, Half-duplex, or Simplex (@SPI_BusConfig) */
    uint8_t SclkSpeed;            /* Clock prescaler / Baud rate control (@SPI_SclkSpeed) */
    uint8_t DFF;                  /* Data Frame Format: 8-bit or 16-bit (@SPI_DFF)       */
    uint8_t CPOL;                 /* Clock Polarity: Idle low or Idle high (@SPI_CPOL)   */
    uint8_t CPHA;                 /* Clock Phase: First edge or Second edge (@SPI_CPHA)  */
    uint8_t SSM;                  /* Software Slave Management: Enable/Disable (@SPI_SSM) */
} SPI_Config_t;

typedef struct {
	SPI_RegDef_t *Instance;       /* SPI base address (SPI1 ... SPI4)                    */
	SPI_Config_t Config;
	uint8_t      *pTxBuffer;
	uint8_t      *pRxBuffer;
	uint32_t     TxLen;
	uint32_t     RxLen;
	uint8_t      TxState;
	uint8_t      RxState;
} SPI_Handle_t;

/*
 * @SPI_DeviceMode
 */
#define SPI_DEVICE_MODE_SLAVE               0
#define SPI_DEVICE_MODE_MASTER              1

/*
 * @SPI_BusConfig
 */
#define SPI_BUS_CONFIG_FULL_DUPLEX          1
#define SPI_BUS_CONFIG_HALF_DUPLEX          2
#define SPI_BUS_CONFIG_SIMPLEX_RXONLY       3

/*
 * @SPI_SclkSpeed (Baud Rate Prescalers)
 */
#define SPI_SCLK_SPEED_DIV2                 0
#define SPI_SCLK_SPEED_DIV4                 1
#define SPI_SCLK_SPEED_DIV8                 2
#define SPI_SCLK_SPEED_DIV16                3
#define SPI_SCLK_SPEED_DIV32                4
#define SPI_SCLK_SPEED_DIV64                5
#define SPI_SCLK_SPEED_DIV128               6
#define SPI_SCLK_SPEED_DIV256               7

/*
 * @SPI_DFF (Data Frame Format)
 */
#define SPI_DFF_8BITS                       0
#define SPI_DFF_16BITS                      1

/*
 * @SPI_CPOL (Clock Polarity)
 */
#define SPI_CPOL_LOW                        0
#define SPI_CPOL_HIGH                       1

/*
 * @SPI_CPHA (Clock Phase)
 */
#define SPI_CPHA_LOW                        0  /* Data captured on first clock transition  */
#define SPI_CPHA_HIGH                       1  /* Data captured on second clock transition */

/*
 * @SPI_SSM (Software Slave Management)
 */
#define SPI_SSM_DI                          0  /* Hardware slave management (NSS pin used) */
#define SPI_SSM_EN                          1  /* Software slave management enabled        */

#define SPI_SR_RXNE                         0 /* Bit 0: Receive buffer not empty */
#define SPI_SR_TXE                          1 /* Bit 1: Transmit buffer empty */
#define SPI_SR_MODF                         5 /* Bit 5: Mode fault              */
#define SPI_SR_OVR                          6 /* Bit 6: Overrun flag            */
#define SPI_SR_BSY                          7 /* Bit 7: Busy flag              */

/*
 * SPI CR1 bit positions
 */
#define SPI_CR1_CPHA                        0
#define SPI_CR1_CPOL                        1
#define SPI_CR1_MSTR                        2
#define SPI_CR1_BR                          3
#define SPI_CR1_SPE                         6
#define SPI_CR1_SSI                         8
#define SPI_CR1_SSM                         9
#define SPI_CR1_RXONLY                     10
#define SPI_CR1_DFF                        11
#define SPI_CR1_BIDIMODE                   15

/*
 * SPI CR2 bit positions
 */
#define SPI_CR2_SSOE                        2
#define SPI_CR2_ERRIE                       5
#define SPI_CR2_RXNEIE                      6
#define SPI_CR2_TXEIE                       7

/**
 * SPI application states
 */
#define SPI_READY   0
#define SPI_BUSY_IN_TX 1
#define SPI_BUSY_IN_RX 2

/**
 * SPI Events
 */
#define SPI_EVENT_TX_CMPLT   1
#define SPI_EVENT_RX_CMPLT   2
#define SPI_EVENT_OVR_ERR    3

/* ========================================================================== */
/*                       DRIVER API FOR SPI PERIPHERAL                        */
/* ========================================================================== */

/**
 * @brief  Enables or Disables peripheral clock for the given SPI peripheral
 * @param  pSPIx: Base address of the SPI peripheral (SPI1, SPI2, etc.)
 * @param  EnorDi: Enable (ENABLE) or Disable (DISABLE) macros
 */
void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi);

/**
 * @brief  Initializes the SPI peripheral according to configuration parameters
 * @param  pSPIHandle: Pointer to the SPI handle structure containing base address and config
 */
void SPI_Init(SPI_Handle_t *pSPIHandle);

/**
 * @brief  De-initializes the SPI peripheral registers back to reset values
 * @param  pSPIx: Base address of the SPI peripheral
 */
void SPI_DeInit(SPI_RegDef_t *pSPIx);

/**
 * @brief  Transmits data over the SPI bus (Blocking/Polling method)
 * @param  pSPIx: Base address of the SPI peripheral
 * @param  pTxBuffer: Pointer to the data byte buffer to be transmitted
 * @param  Len: Length of data bytes to send
 */
void SPI_SendData(SPI_RegDef_t *pSPIx, uint8_t *pTxBuffer, uint32_t Len);

/**
 * @brief  Receives data over the SPI bus (Blocking/Polling method)
 * @param  pSPIx: Base address of the SPI peripheral
 * @param  pRxBuffer: Pointer to memory buffer where received data will be stored
 * @param  Len: Length of data bytes to receive
 */
void SPI_ReceiveData(SPI_RegDef_t *pSPIx, uint8_t *pRxBuffer, uint32_t Len);

/**
 * @brief  Configures the NVIC interrupt controller settings for SPI interrupts
 * @param  IRQNumber: Interrupt Request Number associated with the SPI peripheral
 * @param  EnorDi: Enable or Disable interrupt macro
 */
void SPI_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi);

/**
 * @brief  Configures the execution priority level for the specific SPI interrupt line
 * @param  IRQNumber: Interrupt Request Number associated with the SPI peripheral
 * @param  IRQPriority: Priority value assignment (0 to 15, depending on NVIC configuration)
 */
void SPI_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);

/**
 * @brief  Processes current hardware status flags during an active SPI ISR routine
 * @param  pSPIHandle: Pointer to the active SPI handle structure tracking the transfer
 */
void SPI_IRQHandling(SPI_Handle_t *pSPIHandle);

/**
 * @brief  Enables or disables the SPI peripheral (CR1 SPE bit)
 * @param  pSPIx: Base address of the SPI peripheral
 * @param  EnorDi: ENABLE or DISABLE macros
 */
void SPI_PeripheralControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi);

/**
 * @brief  Reports whether the SPI is still shifting a frame
 * @param  pSPIx: Base address of the SPI peripheral
 * @return 1 while busy, 0 when idle
 */
int SPI_IsBusy(SPI_RegDef_t *pSPIx);

/**
 * @brief  Starts an interrupt driven transmission
 * @return State before the call. SPI_READY means the transfer was accepted
 */
uint8_t SPI_SendDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pTxBuffer, uint32_t Len);

/**
 * @brief  Starts an interrupt driven reception
 * @return State before the call. SPI_READY means the transfer was accepted
 */
uint8_t SPI_ReceiveDataIT(SPI_Handle_t *pSPIHandle, uint8_t *pRxBuffer, uint32_t Len);

/**
 * @brief  Application-level notification of SPI events. Declared weak in the
 *         driver; override it in the application to receive SPI_EVENT_* codes.
 * @param  pSPIHandle: Handle the event belongs to
 * @param  AppEv: One of SPI_EVENT_TX_CMPLT, SPI_EVENT_RX_CMPLT, SPI_EVENT_OVR_ERR
 */
void SPI_ApplicationEventCallback(SPI_Handle_t *pSPIHandle, uint8_t AppEv);

#endif /* INC_STM32F446XX_SPI_H_ */

