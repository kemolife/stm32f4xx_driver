/*
 * stm32f446xx_uart.h
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_UART_H_
#define INC_STM32F446XX_UART_H_

#include "stm32f446xx.h"

/**
 * @brief UART Peripheral Configuration structure definition
 */
typedef struct {
    uint8_t  Mode;                /* TX only, RX only or TX and RX (@UART_Mode)             */
    uint32_t Baud;                /* Baud rate in bits per second (@UART_Baud)              */
    uint8_t  NoOfStopBits;        /* Number of stop bits (@UART_NoOfStopBits)               */
    uint8_t  WordLength;          /* 8 or 9 data bits (@UART_WordLength)                    */
    uint8_t  ParityControl;       /* None, even or odd parity (@UART_ParityControl)         */
    uint8_t  HWFlowControl;       /* None, CTS, RTS or CTS+RTS (@UART_HWFlowControl)        */
    uint8_t  OverSampling;        /* Oversampling by 16 or by 8 (@UART_OverSampling)        */
} UART_Config_t;

typedef struct {
	UART_RegDef_t *Instance;      /* USART/UART base address (USART1 ... USART6)            */
	UART_Config_t Config;
	uint8_t       *pTxBuffer;
	uint8_t       *pRxBuffer;
	uint32_t      TxLen;
	uint32_t      RxLen;
	uint8_t       TxBusyState;    /* TX side state, full duplex (@UART_ApplicationStates)   */
	uint8_t       RxBusyState;    /* RX side state, full duplex (@UART_ApplicationStates)   */
} UART_Handle_t;

/*
 * @UART_Mode
 */
#define UART_MODE_ONLY_TX                   0
#define UART_MODE_ONLY_RX                   1
#define UART_MODE_TXRX                      2

/*
 * @UART_Baud
 */
#define UART_STD_BAUD_1200                  1200
#define UART_STD_BAUD_2400                  2400
#define UART_STD_BAUD_9600                  9600
#define UART_STD_BAUD_19200                 19200
#define UART_STD_BAUD_38400                 38400
#define UART_STD_BAUD_57600                 57600
#define UART_STD_BAUD_115200                115200
#define UART_STD_BAUD_230400                230400
#define UART_STD_BAUD_460800                460800
#define UART_STD_BAUD_921600                921600
#define UART_STD_BAUD_2M                    2000000
#define UART_STD_BAUD_3M                    3000000

/*
 * @UART_NoOfStopBits (raw value of CR2 STOP[13:12])
 */
#define UART_STOPBITS_1                     0
#define UART_STOPBITS_0_5                   1
#define UART_STOPBITS_2                     2
#define UART_STOPBITS_1_5                   3

/*
 * @UART_WordLength (raw value of CR1 M)
 */
#define UART_WORDLEN_8BITS                  0
#define UART_WORDLEN_9BITS                  1

/*
 * @UART_ParityControl
 */
#define UART_PARITY_DISABLE                 0
#define UART_PARITY_EN_EVEN                 1
#define UART_PARITY_EN_ODD                  2

/*
 * @UART_HWFlowControl
 */
#define UART_HW_FLOW_CTRL_NONE              0
#define UART_HW_FLOW_CTRL_CTS               1
#define UART_HW_FLOW_CTRL_RTS               2
#define UART_HW_FLOW_CTRL_CTS_RTS           3

/*
 * @UART_OverSampling (raw value of CR1 OVER8)
 */
#define UART_OVERSAMPLING_16                0
#define UART_OVERSAMPLING_8                 1

/*
 * UART status flag masks, for UART_GetFlagStatus / UART_ClearFlag
 */
#define UART_FLAG_PE                        (1U << USART_SR_PE)
#define UART_FLAG_FE                        (1U << USART_SR_FE)
#define UART_FLAG_NF                        (1U << USART_SR_NF)
#define UART_FLAG_ORE                       (1U << USART_SR_ORE)
#define UART_FLAG_IDLE                      (1U << USART_SR_IDLE)
#define UART_FLAG_RXNE                      (1U << USART_SR_RXNE)
#define UART_FLAG_TC                        (1U << USART_SR_TC)
#define UART_FLAG_TXE                       (1U << USART_SR_TXE)
#define UART_FLAG_LBD                       (1U << USART_SR_LBD)
#define UART_FLAG_CTS                       (1U << USART_SR_CTS)

/*
 * Return values of UART_GetFlagStatus
 */
#define UART_FLAG_RESET                     0
#define UART_FLAG_SET                       1

/**
 * UART application states (@UART_ApplicationStates)
 */
#define UART_READY                          0
#define UART_BUSY_IN_RX                     1
#define UART_BUSY_IN_TX                     2

/**
 * UART Events
 */
#define UART_EVENT_TX_CMPLT                 1
#define UART_EVENT_RX_CMPLT                 2
#define UART_EVENT_IDLE                     3
#define UART_EVENT_CTS                      4

/**
 * UART Errors
 */
#define UART_ERROR_PE                       5
#define UART_ERROR_FE                       6
#define UART_ERROR_NE                       7
#define UART_ERROR_ORE                      8

/* ========================================================================== */
/*                       DRIVER API FOR UART PERIPHERAL                       */
/* ========================================================================== */

/**
 * @brief  Enables or Disables peripheral clock for the given USART/UART peripheral
 * @param  pUARTx: Base address of the peripheral (USART1, USART2, USART3, UART4, UART5, USART6)
 * @param  State: DRV_ENABLE or DRV_DISABLE
 */
void UART_PeriClockControl(UART_RegDef_t *pUARTx, DRV_State_t State);

/**
 * @brief  Initializes the UART peripheral according to configuration parameters
 * @param  pUARTHandle: Pointer to the UART handle structure containing base address and config
 */
void UART_Init(UART_Handle_t *pUARTHandle);

/**
 * @brief  De-initializes the UART peripheral registers back to reset values
 * @param  pUARTx: Base address of the UART peripheral
 */
void UART_DeInit(UART_RegDef_t *pUARTx);

/**
 * @brief  Enables or disables the UART peripheral (CR1 UE bit)
 * @param  pUARTx: Base address of the UART peripheral
 * @param  State: DRV_ENABLE or DRV_DISABLE
 */
void UART_PeripheralControl(UART_RegDef_t *pUARTx, DRV_State_t State);

/**
 * @brief  Programs BRR for the requested baud rate from the current bus clock
 * @param  pUARTx: Base address of the UART peripheral
 * @param  BaudRate: Baud rate in bits per second (@UART_Baud)
 */
void UART_SetBaudRate(UART_RegDef_t *pUARTx, uint32_t BaudRate);

/**
 * @brief  Reads one status flag from SR
 * @param  pUARTx: Base address of the UART peripheral
 * @param  FlagName: One of the UART_FLAG_* masks
 * @return UART_FLAG_SET or UART_FLAG_RESET
 */
uint8_t UART_GetFlagStatus(UART_RegDef_t *pUARTx, uint32_t FlagName);

/**
 * @brief  Clears one status flag in SR
 * @param  pUARTx: Base address of the UART peripheral
 * @param  FlagName: One of the UART_FLAG_* masks
 */
void UART_ClearFlag(UART_RegDef_t *pUARTx, uint32_t FlagName);

/**
 * @brief  Transmits data over the UART (Blocking/Polling method)
 * @param  pUARTHandle: Pointer to the UART handle structure
 * @param  pTxBuffer: Pointer to the data buffer to be transmitted
 * @param  Len: Number of frames to send
 * @param  Timeout: Maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 * @return DRV_OK, DRV_ERROR (bad argument) or DRV_TIMEOUT
 */
DRV_Status_t UART_SendData(UART_Handle_t *pUARTHandle, uint8_t *pTxBuffer, uint32_t Len, uint32_t Timeout);

/**
 * @brief  Receives data over the UART (Blocking/Polling method)
 * @param  pUARTHandle: Pointer to the UART handle structure
 * @param  pRxBuffer: Pointer to memory buffer where received data will be stored
 * @param  Len: Number of frames to receive
 * @param  Timeout: Maximum time for the whole call in ms, DRV_MAX_DELAY = forever
 * @return DRV_OK, DRV_ERROR (bad argument) or DRV_TIMEOUT
 */
DRV_Status_t UART_ReceiveData(UART_Handle_t *pUARTHandle, uint8_t *pRxBuffer, uint32_t Len, uint32_t Timeout);

/**
 * @brief  Starts an interrupt driven transmission
 * @return DRV_OK (started), DRV_BUSY (still running) or DRV_ERROR
 */
DRV_Status_t UART_SendDataIT(UART_Handle_t *pUARTHandle, uint8_t *pTxBuffer, uint32_t Len);

/**
 * @brief  Starts an interrupt driven reception
 * @return DRV_OK (started), DRV_BUSY (still running) or DRV_ERROR
 */
DRV_Status_t UART_ReceiveDataIT(UART_Handle_t *pUARTHandle, uint8_t *pRxBuffer, uint32_t Len);

/**
 * @brief  Configures the NVIC interrupt controller settings for UART interrupts
 * @param  IRQNumber: Interrupt Request Number associated with the UART peripheral
 * @param  State: DRV_ENABLE or DRV_DISABLE
 */
void UART_IRQInterruptConfig(uint8_t IRQNumber, DRV_State_t State);

/**
 * @brief  Configures the execution priority level for the specific UART interrupt line
 * @param  IRQNumber: Interrupt Request Number associated with the UART peripheral
 * @param  IRQPriority: Priority value assignment (0 to 15)
 */
void UART_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);

/**
 * @brief  Processes the UART interrupt sources (TXE, TC, RXNE, IDLE, CTS, PE, FE, NF, ORE)
 * @param  pUARTHandle: Pointer to the active UART handle structure tracking the transfer
 */
void UART_IRQHandling(UART_Handle_t *pUARTHandle);

/**
 * @brief  Application-level notification of UART events. Declared weak in the
 *         driver; override it in the application to receive event and error codes.
 * @param  pUARTHandle: Handle the event belongs to
 * @param  AppEv: One of the UART_EVENT_* or UART_ERROR_* macros
 */
void UART_ApplicationEventCallback(UART_Handle_t *pUARTHandle, uint8_t AppEv);

#endif /* INC_STM32F446XX_UART_H_ */
