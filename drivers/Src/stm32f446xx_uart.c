/*
 * stm32f446xx_uart.c
 *
 *  Created on: 28 Sept 2026
 *  Author: vitaliiantoniuk
 *
 *  SKELETON ONLY. Every public function below compiles but does nothing yet.
 *  The numbered comments inside each body are the steps to implement. They
 *  follow RM0390 section 25 (USART). Validate each step against the reference
 *  manual, then replace the TODO with code. Demo apps: Src/uart_tx.c and
 *  Src/uart_cmd_handling_it.c. On-target tests: tests/target/uart_driver_test.c.
 */

#include <stddef.h>
#include <stdint.h>
#include "stm32f446xx.h"

static void UART_HandleTXEInterrupt(UART_Handle_t *pUARTHandle);
static void UART_HandleTCInterrupt(UART_Handle_t *pUARTHandle);
static void UART_HandleRXNEInterrupt(UART_Handle_t *pUARTHandle);

/******************************************************************************************
 * @fn                 - UART_PeriClockControl
 *
 * @brief              - This function enables or disables peripheral clock for the given UARTx
 *
 * @param[in]          - base address of the uart peripheral
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - USART1 and USART6 sit on APB2, the other four on APB1
 *
 ******************************************************************************************/
void UART_PeriClockControl(UART_RegDef_t *pUARTx, uint8_t EnorDi) {
	/* 1. If EnorDi == ENABLE, compare pUARTx against USART1, USART2, USART3,
	 *    UART4, UART5, USART6 and call the matching *_PCLK_EN() macro.
	 * 2. Otherwise call the matching *_PCLK_DI() macro.
	 * 3. An unknown pointer must do nothing (no default branch that touches RCC). */

	/* TODO: implement */
	(void)pUARTx;
	(void)EnorDi;
}

/******************************************************************************************
 * @fn                 - UART_Init
 *
 * @brief              - Initializes the given UARTx with specified configurations
 *
 * @param[in]          - pointer to the UART Handle structure
 *
 * @return             - none
 *
 * @Note               - call this while UE is 0, before UART_PeripheralControl(..., ENABLE).
 *                       The peripheral clock must already be on
 *
 ******************************************************************************************/
void UART_Init(UART_Handle_t *pUARTHandle) {
	/* ---------------- CR1 ---------------- */
	/* 1. Start from tempreg = 0 (32-bit, OVER8 lives at bit 15).
	 * 2. Mode:
	 *      UART_MODE_ONLY_RX -> set RE
	 *      UART_MODE_ONLY_TX -> set TE
	 *      UART_MODE_TXRX    -> set RE and TE
	 * 3. WordLength -> write it to M (bit 12).
	 * 4. ParityControl:
	 *      UART_PARITY_EN_EVEN -> set PCE, clear PS
	 *      UART_PARITY_EN_ODD  -> set PCE, set PS
	 *      UART_PARITY_DISABLE -> leave both 0
	 *    Note: parity takes the MSB of the frame. 8-bit + parity = 7 data bits.
	 * 5. OverSampling -> write it to OVER8 (bit 15). BRR depends on it,
	 *    so CR1 must be written before step 10.
	 * 6. Write tempreg to CR1. Do NOT set UE here. */

	/* ---------------- CR2 ---------------- */
	/* 7. tempreg = 0, put NoOfStopBits into STOP[13:12], write CR2.
	 *    Values 0.5 and 1.5 stop bits are for smartcard mode; UART4/UART5
	 *    support them too, but they are rarely needed. */

	/* ---------------- CR3 ---------------- */
	/* 8. tempreg = 0, then HWFlowControl:
	 *      UART_HW_FLOW_CTRL_CTS     -> set CTSE
	 *      UART_HW_FLOW_CTRL_RTS     -> set RTSE
	 *      UART_HW_FLOW_CTRL_CTS_RTS -> set both
	 *    UART4 and UART5 have no CTS/RTS pins, so ignore flow control for them.
	 * 9. Write tempreg to CR3. */

	/* ---------------- BRR ---------------- */
	/* 10. Call UART_SetBaudRate(pUARTHandle->Instance, Baud). */

	/* TODO: implement */
	(void)pUARTHandle;
}

/******************************************************************************************
 * @fn                 - UART_DeInit
 *
 * @brief              - Resets all registers of a given UARTx back to their reset values
 *
 * @param[in]          - base address of the UART peripheral
 *
 * @return             - none
 *
 * @Note               - drives the RCC reset line, so it clears UE and every config bit
 *
 ******************************************************************************************/
void UART_DeInit(UART_RegDef_t *pUARTx) {
	/* 1. Compare pUARTx against the six instances and call the matching
	 *    *_REG_RESET() macro from stm32f446xx.h. */

	/* TODO: implement */
	(void)pUARTx;
}

/******************************************************************************************
 * @fn                 - UART_PeripheralControl
 *
 * @brief              - Enables or disables the UART peripheral (CR1 UE bit)
 *
 * @param[in]          - base address of the UART peripheral
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - before disabling, wait for TC=1 so the last frame is not cut
 *
 ******************************************************************************************/
void UART_PeripheralControl(UART_RegDef_t *pUARTx, uint8_t EnorDi) {
	/* 1. ENABLE  -> CR1 |=  (1 << USART_CR1_UE)
	 * 2. DISABLE -> CR1 &= ~(1 << USART_CR1_UE) */

	/* TODO: implement */
	(void)pUARTx;
	(void)EnorDi;
}

/******************************************************************************************
 * @fn                 - UART_SetBaudRate
 *
 * @brief              - Programs BRR for the requested baud rate
 *
 * @param[in]          - base address of the UART peripheral
 * @param[in]          - baud rate in bits per second
 *
 * @return             - none
 *
 * @Note               - reads OVER8 from CR1, so CR1 must be configured first
 *
 ******************************************************************************************/
void UART_SetBaudRate(UART_RegDef_t *pUARTx, uint32_t BaudRate) {
	/* 1. Get the bus clock:
	 *      USART1, USART6 -> RCC_GetPCLK2Value()
	 *      others         -> RCC_GetPCLK1Value()
	 * 2. RM0390: USARTDIV = fPCLK / (8 * (2 - OVER8) * BaudRate)
	 *    Use integers only, scaled by 100 to keep two decimals:
	 *      OVER8 = 1: usartdiv = (25 * PCLKx) / (2 * BaudRate)
	 *      OVER8 = 0: usartdiv = (25 * PCLKx) / (4 * BaudRate)
	 *    Check: 25 * 90 MHz = 2.25e9, still fits in uint32_t.
	 * 3. mantissa = usartdiv / 100
	 * 4. fraction_x100 = usartdiv - (mantissa * 100)
	 * 5. Round the fraction to the register resolution:
	 *      OVER8 = 1: fraction = ((fraction_x100 * 8)  + 50) / 100, keep 3 bits (& 0x07)
	 *      OVER8 = 0: fraction = ((fraction_x100 * 16) + 50) / 100, keep 4 bits (& 0x0F)
	 *    If rounding overflows (result 8 or 16), add 1 to mantissa and set fraction 0.
	 *    With OVER8 = 1, BRR bit 3 must stay 0.
	 * 6. BRR = (mantissa << USART_BRR_DIV_MANTISSA) | fraction
	 *
	 * Golden values to validate by hand (PCLK = 16 MHz, OVER8 = 0):
	 *      9600   -> USARTDIV 104.1875 -> BRR 0x0683
	 *      115200 -> USARTDIV   8.6875 -> BRR 0x008B */

	/* TODO: implement */
	(void)pUARTx;
	(void)BaudRate;
}

/******************************************************************************************
 * @fn                 - UART_GetFlagStatus
 *
 * @brief              - Returns the state of one SR flag
 *
 * @param[in]          - base address of the UART peripheral
 * @param[in]          - one of the UART_FLAG_* masks
 *
 * @return             - UART_FLAG_SET or UART_FLAG_RESET
 *
 * @Note               - reading SR is the first half of several clear sequences
 *                       (PE, FE, NF, ORE, IDLE: read SR then read DR)
 *
 ******************************************************************************************/
uint8_t UART_GetFlagStatus(UART_RegDef_t *pUARTx, uint32_t FlagName) {
	/* 1. If (SR & FlagName) is non-zero return UART_FLAG_SET.
	 * 2. Otherwise return UART_FLAG_RESET. */

	/* TODO: implement */
	(void)pUARTx;
	(void)FlagName;
	return UART_FLAG_RESET;
}

/******************************************************************************************
 * @fn                 - UART_ClearFlag
 *
 * @brief              - Clears one SR flag
 *
 * @param[in]          - base address of the UART peripheral
 * @param[in]          - one of the UART_FLAG_* masks
 *
 * @return             - none
 *
 * @Note               - only CTS, LBD, TC and RXNE are rc_w0 (write 0 to clear).
 *                       Writing 1 to the other bits has no effect
 *
 ******************************************************************************************/
void UART_ClearFlag(UART_RegDef_t *pUARTx, uint32_t FlagName) {
	/* 1. SR &= ~FlagName  (write 0 only to the flag, 1 elsewhere).
	 * 2. Do not use this for PE, FE, NF, ORE, IDLE. Those need the
	 *    read SR -> read DR sequence instead. Decide if you want to handle
	 *    that here or leave it to the IRQ handler. */

	/* TODO: implement */
	(void)pUARTx;
	(void)FlagName;
}

/******************************************************************************************
 * @fn                 - UART_SendData
 *
 * @brief              - Sends Len frames over the UART
 *
 * @param[in]          - pointer to the UART Handle structure
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of frames to send
 *
 * @return             - none
 *
 * @Note               - blocking call. It returns only when the last frame has left the
 *                       shift register (TC = 1)
 *
 ******************************************************************************************/
void UART_SendData(UART_Handle_t *pUARTHandle, uint8_t *pTxBuffer, uint32_t Len) {
	/* 1. Loop Len times:
	 * 2.   Wait until TXE = 1 (DR is free, previous byte moved to shift register).
	 * 3.   If WordLength == 9 bits:
	 *        - load 2 bytes: DR = (*(uint16_t *)pTxBuffer) & 0x01FF
	 *        - no parity: 9 data bits, advance pTxBuffer by 2
	 *        - parity on: bit 8 is parity (hardware), 8 data bits, advance by 1
	 *      Else (8 bits):
	 *        - DR = *pTxBuffer & 0xFF
	 *        - parity on: only 7 data bits, hardware replaces bit 7
	 *        - advance pTxBuffer by 1
	 * 4. After the loop wait until TC = 1. Only then is the last frame fully out
	 *    and it is safe to disable UE or switch the line direction. */

	/* TODO: implement */
	(void)pUARTHandle;
	(void)pTxBuffer;
	(void)Len;
}

/******************************************************************************************
 * @fn                 - UART_ReceiveData
 *
 * @brief              - Receives Len frames over the UART
 *
 * @param[in]          - pointer to the UART Handle structure
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of frames to receive
 *
 * @return             - none
 *
 * @Note               - blocking call with no timeout. It hangs if no data arrives
 *
 ******************************************************************************************/
void UART_ReceiveData(UART_Handle_t *pUARTHandle, uint8_t *pRxBuffer, uint32_t Len) {
	/* 1. Loop Len times:
	 * 2.   Wait until RXNE = 1.
	 * 3.   If WordLength == 9 bits:
	 *        - no parity: *(uint16_t *)pRxBuffer = DR & 0x01FF, advance by 2
	 *        - parity on: *pRxBuffer = DR & 0xFF, advance by 1
	 *      Else (8 bits):
	 *        - no parity: *pRxBuffer = DR & 0xFF
	 *        - parity on: *pRxBuffer = DR & 0x7F (bit 7 is the parity bit)
	 *        - advance by 1
	 *    Reading DR clears RXNE. */

	/* TODO: implement */
	(void)pUARTHandle;
	(void)pRxBuffer;
	(void)Len;
}

/******************************************************************************************
 * @fn                 - UART_SendDataIT
 *
 * @brief              - Starts an interrupt driven transmission
 *
 * @param[in]          - pointer to the UART Handle structure
 * @param[in]          - pointer to the transmit buffer
 * @param[in]          - number of frames to send
 *
 * @return             - the TX state before the call. UART_READY means the transfer was
 *                       accepted, UART_BUSY_IN_TX means it was rejected
 *
 * @Note               - the buffer must stay valid until the UART_EVENT_TX_CMPLT callback
 *
 ******************************************************************************************/
uint8_t UART_SendDataIT(UART_Handle_t *pUARTHandle, uint8_t *pTxBuffer, uint32_t Len) {
	/* 1. txstate = pUARTHandle->TxBusyState
	 * 2. If txstate != UART_BUSY_IN_TX:
	 *      a. save pTxBuffer and Len in the handle
	 *      b. TxBusyState = UART_BUSY_IN_TX
	 *      c. set TXEIE  -> the ISR is entered at once, because TXE is already 1
	 *      d. set TCIE   -> used to know when the last frame has left the wire
	 * 3. return txstate */

	/* TODO: implement */
	(void)pUARTHandle;
	(void)pTxBuffer;
	(void)Len;
	return UART_READY;
}

/******************************************************************************************
 * @fn                 - UART_ReceiveDataIT
 *
 * @brief              - Starts an interrupt driven reception
 *
 * @param[in]          - pointer to the UART Handle structure
 * @param[in]          - pointer to the receive buffer
 * @param[in]          - number of frames to receive
 *
 * @return             - the RX state before the call. UART_READY means the transfer was
 *                       accepted, UART_BUSY_IN_RX means it was rejected
 *
 * @Note               - the buffer must stay valid until the UART_EVENT_RX_CMPLT callback
 *
 ******************************************************************************************/
uint8_t UART_ReceiveDataIT(UART_Handle_t *pUARTHandle, uint8_t *pRxBuffer, uint32_t Len) {
	/* 1. rxstate = pUARTHandle->RxBusyState
	 * 2. If rxstate != UART_BUSY_IN_RX:
	 *      a. save pRxBuffer and Len in the handle
	 *      b. RxBusyState = UART_BUSY_IN_RX
	 *      c. set RXNEIE (this also enables the ORE interrupt)
	 *      d. optional: set CR3 EIE for FE/NF, and CR1 PEIE if parity is on
	 * 3. return rxstate */

	/* TODO: implement */
	(void)pUARTHandle;
	(void)pRxBuffer;
	(void)Len;
	return UART_READY;
}

/******************************************************************************************
 * @fn                 - UART_IRQInterruptConfig
 *
 * @brief              - Enables or disables the interrupt processing for a given IRQ number in NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - ENABLE or DISABLE macros
 *
 * @return             - none
 *
 * @Note               - UART IRQs are 37..39, 52, 53 and 71
 *
 ******************************************************************************************/
void UART_IRQInterruptConfig(uint8_t IRQNumber, uint8_t EnorDi) {
	/* 1. One line: call NVIC_IRQInterruptConfig(IRQNumber, EnorDi).
	 *    The NVIC logic lives in stm32f446xx_nvic.c for every driver,
	 *    see GPIO_IRQInterruptConfig for the same wrapper. */

	/* TODO: implement */
	(void)IRQNumber;
	(void)EnorDi;
}

/******************************************************************************************
 * @fn                 - UART_IRQPriorityConfig
 *
 * @brief              - Configures the priority level of a given IRQ number in the NVIC
 *
 * @param[in]          - IRQ number to configure
 * @param[in]          - priority level value
 *
 * @return             - none
 *
 * @Note               - priority 0 (most urgent) .. 15 (least urgent)
 *
 ******************************************************************************************/
void UART_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority) {
	/* 1. One line: call NVIC_IRQPriorityConfig(IRQNumber, IRQPriority). */

	/* TODO: implement */
	(void)IRQNumber;
	(void)IRQPriority;
}

/******************************************************************************************
 * @fn                 - UART_IRQHandling
 *
 * @brief              - Handles every UART interrupt source
 *
 * @param[in]          - pointer to the active UART Handle structure
 *
 * @return             - none
 *
 * @Note               - should be called inside the USARTx_IRQHandler / UARTx_IRQHandler
 *                       vector. One vector serves all sources, so check each flag AND its
 *                       enable bit
 *
 ******************************************************************************************/
void UART_IRQHandling(UART_Handle_t *pUARTHandle) {
	/* Read SR once into a local, and CR1 / CR3 for the enable bits.
	 *
	 * 1. TC  && TCIE:
	 *      if TxBusyState == UART_BUSY_IN_TX and TxLen == 0 -> UART_HandleTCInterrupt()
	 *
	 * 2. TXE && TXEIE:
	 *      if TxBusyState == UART_BUSY_IN_TX -> UART_HandleTXEInterrupt()
	 *
	 * 3. RXNE && RXNEIE:
	 *      if RxBusyState == UART_BUSY_IN_RX -> UART_HandleRXNEInterrupt()
	 *
	 * 4. CTS && CTSIE (CR3):   (not on UART4/UART5)
	 *      clear CTS (write 0), callback UART_EVENT_CTS
	 *
	 * 5. IDLE && IDLEIE:
	 *      clear with read SR -> read DR, callback UART_EVENT_IDLE
	 *
	 * 6. ORE && RXNEIE:
	 *      do NOT clear here if you want the app to read the last byte.
	 *      callback UART_ERROR_ORE. The app clears it with read SR -> read DR.
	 *
	 * 7. EIE (CR3) set, check FE, NF, ORE (multibuffer):
	 *      FE -> UART_ERROR_FE, NF -> UART_ERROR_NE, ORE -> UART_ERROR_ORE
	 *      all cleared with read SR -> read DR
	 *
	 * 8. PE && PEIE:
	 *      wait RXNE, then read SR -> read DR, callback UART_ERROR_PE */

	/* TODO: implement */
	(void)pUARTHandle;
}

/******************************************************************************************
 * @fn                 - UART_HandleTXEInterrupt
 *
 * @brief              - Feeds the next frame to DR during an IT transmission
 *
 * @param[in]          - pointer to the active UART Handle structure
 *
 * @return             - none
 *
 * @Note               - the transfer is closed in the TC handler, not here
 *
 ******************************************************************************************/
static void UART_HandleTXEInterrupt(UART_Handle_t *pUARTHandle) {
	/* 1. If TxLen > 0: write one frame to DR (same 8/9-bit and parity rules as
	 *    UART_SendData), advance pTxBuffer, decrement TxLen.
	 * 2. If TxLen == 0 after that: clear TXEIE. Otherwise TXE fires forever,
	 *    because DR stays empty. TC will fire once the shift register is done. */

	/* TODO: implement */
	(void)pUARTHandle;
}

/******************************************************************************************
 * @fn                 - UART_HandleTCInterrupt
 *
 * @brief              - Closes an IT transmission when the last frame has left the wire
 *
 * @param[in]          - pointer to the active UART Handle structure
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
static void UART_HandleTCInterrupt(UART_Handle_t *pUARTHandle) {
	/* 1. Clear TC (write 0 to SR TC).
	 * 2. Clear TCIE.
	 * 3. TxBusyState = UART_READY, pTxBuffer = NULL, TxLen = 0.
	 * 4. UART_ApplicationEventCallback(pUARTHandle, UART_EVENT_TX_CMPLT). */

	/* TODO: implement */
	(void)pUARTHandle;
}

/******************************************************************************************
 * @fn                 - UART_HandleRXNEInterrupt
 *
 * @brief              - Drains one frame from DR during an IT reception
 *
 * @param[in]          - pointer to the active UART Handle structure
 *
 * @return             - none
 *
 * @Note               - none
 *
 ******************************************************************************************/
static void UART_HandleRXNEInterrupt(UART_Handle_t *pUARTHandle) {
	/* 1. Read one frame from DR (same 8/9-bit and parity rules as
	 *    UART_ReceiveData), advance pRxBuffer, decrement RxLen.
	 * 2. If RxLen == 0:
	 *      a. clear RXNEIE
	 *      b. RxBusyState = UART_READY, pRxBuffer = NULL
	 *      c. UART_ApplicationEventCallback(pUARTHandle, UART_EVENT_RX_CMPLT) */

	/* TODO: implement */
	(void)pUARTHandle;
}

__attribute__((weak)) void UART_ApplicationEventCallback(UART_Handle_t *pUARTHandle, uint8_t AppEv)
{
    // This is an empty placeholder.
    // The user can override this function in main.c without modifying the driver!
    (void)pUARTHandle;
    (void)AppEv;
}
