/*
 * stm32f446xx_i2c.h
 *
 *  Created on: 13 Sept 2026
 *  Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_I2C_H_
#define INC_STM32F446XX_I2C_H_

#include "stm32f446xx.h"

/**
 * @brief I2C Peripheral Configuration structure definition
 */
typedef struct {
    uint32_t SCLSpeed;            /* Serial clock speed in Hz (@I2C_SCLSpeed)             */
    uint8_t  DeviceAddress;       /* Own address used when acting as a slave (7-bit)      */
    uint8_t  ACKControl;          /* Automatic acknowledge enable/disable (@I2C_ACKControl) */
    uint8_t  FMDutyCycle;         /* Fast mode duty cycle (@I2C_FMDutyCycle)              */
} I2C_Config_t;

typedef struct {
	I2C_RegDef_t *Instance;       /* I2C base address (I2C1 ... I2C3)                     */
	I2C_Config_t Config;
	uint8_t      *pTxBuffer;
	uint8_t      *pRxBuffer;
	uint32_t     TxLen;
	uint32_t     RxLen;
	uint8_t      TxRxState;      /* Communication state (@I2C_ApplicationStates)          */
	uint8_t      DevAddr;        /* Slave address of the device currently addressed       */
	uint32_t     RxSize;         /* Total size of the ongoing reception                   */
	uint8_t      Sr;             /* Repeated start value (@I2C_RepeatedStart)             */
} I2C_Handle_t;

/*
 * @I2C_SCLSpeed
 */
#define I2C_SCL_SPEED_SM                    100000  /* Standard mode: 100 kHz */
#define I2C_SCL_SPEED_FM2K                  200000  /* Fast mode:     200 kHz */
#define I2C_SCL_SPEED_FM4K                  400000  /* Fast mode:     400 kHz */

/*
 * @I2C_ACKControl
 */
#define I2C_ACK_DISABLE                     0
#define I2C_ACK_ENABLE                      1

/*
 * @I2C_FMDutyCycle
 */
#define I2C_FM_DUTY_2                       0  /* Tlow/Thigh = 2  */
#define I2C_FM_DUTY_16_9                    1  /* Tlow/Thigh = 16/9 */

/*
 * @I2C_RepeatedStart
 */
#define I2C_DISABLE_SR                      0  /* Generate a STOP after the transfer   */
#define I2C_ENABLE_SR                       1  /* Keep the bus, issue a repeated START */

/**
 * I2C application states (@I2C_ApplicationStates)
 */
#define I2C_READY                           0
#define I2C_BUSY_IN_RX                      1
#define I2C_BUSY_IN_TX                      2

/**
 * I2C Events
 */
#define I2C_EVENT_TX_CMPLT                  1
#define I2C_EVENT_RX_CMPLT                  2
#define I2C_EVENT_STOP                      3
#define I2C_EVENT_DATA_REQ                  4  /* Slave mode: master requests a byte   */
#define I2C_EVENT_DATA_RCV                  5  /* Slave mode: master sent a byte       */

/**
 * I2C Errors
 */
#define I2C_ERROR_BERR                      6
#define I2C_ERROR_ARLO                      7
#define I2C_ERROR_AF                        8
#define I2C_ERROR_OVR                       9
#define I2C_ERROR_TIMEOUT                  10

/* ========================================================================== */
/*                       DRIVER API FOR I2C PERIPHERAL                        */
/* ========================================================================== */

/**
 * @brief  Enables or Disables peripheral clock for the given I2C peripheral
 * @param  pI2Cx: Base address of the I2C peripheral (I2C1, I2C2, I2C3)
 * @param  EnorDi: Enable (ENABLE) or Disable (DISABLE) macros
 */
void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi);

/**
 * @brief  Initializes the I2C peripheral according to configuration parameters
 * @param  pI2CHandle: Pointer to the I2C handle structure containing base address and config
 */
void I2C_Init(I2C_Handle_t *pI2CHandle);

/**
 * @brief  De-initializes the I2C peripheral registers back to reset values
 * @param  pI2Cx: Base address of the I2C peripheral
 */
void I2C_DeInit(I2C_RegDef_t *pI2Cx);

/**
 * @brief  Sends data to a slave device as bus master (Blocking/Polling method)
 * @param  pI2CHandle: Pointer to the I2C handle structure
 * @param  pTxBuffer: Pointer to the data byte buffer to be transmitted
 * @param  Len: Length of data bytes to send
 * @param  SlaveAddr: 7-bit address of the target slave
 * @param  Sr: I2C_ENABLE_SR to keep the bus, I2C_DISABLE_SR to issue a STOP
 */
void I2C_MasterSendData(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr);

/**
 * @brief  Reads data from a slave device as bus master (Blocking/Polling method)
 * @param  pI2CHandle: Pointer to the I2C handle structure
 * @param  pRxBuffer: Pointer to memory buffer where received data will be stored
 * @param  Len: Length of data bytes to receive
 * @param  SlaveAddr: 7-bit address of the target slave
 * @param  Sr: I2C_ENABLE_SR to keep the bus, I2C_DISABLE_SR to issue a STOP
 */
void I2C_MasterReceiveData(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr);

/**
 * @brief  Configures the NVIC interrupt controller settings for I2C interrupts
 * @param  IRQNumber: Interrupt Request Number associated with the I2C peripheral
 * @param  EnorDi: Enable or Disable interrupt macro
 */
void I2C_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi);

/**
 * @brief  Configures the execution priority level for the specific I2C interrupt line
 * @param  IRQNumber: Interrupt Request Number associated with the I2C peripheral
 * @param  IRQPriority: Priority value assignment (0 to 15, depending on NVIC configuration)
 */
void I2C_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);

/**
 * @brief  Processes the I2C event interrupts (SB, ADDR, BTF, STOPF, TXE, RXNE)
 * @param  pI2CHandle: Pointer to the active I2C handle structure tracking the transfer
 */
void I2C_EV_IRQHandling(I2C_Handle_t *pI2CHandle);

/**
 * @brief  Processes the I2C error interrupts (BERR, ARLO, AF, OVR, TIMEOUT)
 * @param  pI2CHandle: Pointer to the active I2C handle structure tracking the transfer
 */
void I2C_ER_IRQHandling(I2C_Handle_t *pI2CHandle);

/**
 * @brief  Enables or disables the I2C peripheral (CR1 PE bit)
 * @param  pI2Cx: Base address of the I2C peripheral
 * @param  EnorDi: ENABLE or DISABLE macros
 * @note   Call after I2C_Init: FREQ, CCR and TRISE can only be written while PE is 0
 */
void I2C_PeripheralControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi);

/**
 * @brief  Reports whether any transfer is in progress on the bus (SR2 BUSY)
 * @param  pI2Cx: Base address of the I2C peripheral
 * @return 1 between a START and a STOP from any master, 0 when the bus is free
 */
int I2C_IsBusy(I2C_RegDef_t *pI2Cx);

/**
 * @brief  Enables or disables automatic acknowledging on the given I2C peripheral
 * @param  pI2Cx: Base address of the I2C peripheral
 * @param  EnorDi: I2C_ACK_ENABLE or I2C_ACK_DISABLE
 */
void I2C_ManageAcking(I2C_RegDef_t *pI2Cx, uint8_t EnorDi);

/**
 * @brief  Starts an interrupt driven write to a slave as bus master
 * @return State before the call. I2C_READY means the transfer was accepted
 * @note   The buffer must stay valid until the I2C_EVENT_TX_CMPLT callback
 */
uint8_t I2C_MasterSendDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pTxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr);

/**
 * @brief  Starts an interrupt driven read from a slave as bus master
 * @return State before the call. I2C_READY means the transfer was accepted
 * @note   The buffer must stay valid until the I2C_EVENT_RX_CMPLT callback
 */
uint8_t I2C_MasterReceiveDataIT(I2C_Handle_t *pI2CHandle, uint8_t *pRxBuffer, uint32_t Len, uint8_t SlaveAddr, uint8_t Sr);

/**
 * @brief  Sends a single byte while the peripheral is addressed as a slave
 * @param  pI2Cx: Base address of the I2C peripheral
 * @param  data: Byte to place in the data register
 */
void I2C_SlaveSendData(I2C_RegDef_t *pI2Cx, uint8_t data);

/**
 * @brief  Reads a single byte while the peripheral is addressed as a slave
 * @param  pI2Cx: Base address of the I2C peripheral
 * @return Byte read from the data register
 */
uint8_t I2C_SlaveReceiveData(I2C_RegDef_t *pI2Cx);

/**
 * @brief  Application-level notification of I2C events. Declared weak in the
 *         driver; override it in the application to receive event and error codes.
 * @param  pI2CHandle: Handle the event belongs to
 * @param  AppEv: One of the I2C_EVENT_* or I2C_ERROR_* macros
 */
void I2C_ApplicationEventCallback(I2C_Handle_t *pI2CHandle, uint8_t AppEv);

#endif /* INC_STM32F446XX_I2C_H_ */
