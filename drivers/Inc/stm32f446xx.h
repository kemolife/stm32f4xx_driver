/*
 * stm32f446xx.h
 *
 *  Created on: 5 Sept 2026
 *  Author: vitaliiantoniuk
 */
#ifndef INC_STM32F446XX_H_
#define INC_STM32F446XX_H_

#include <stdint.h>

#define FLASH_BASE            0x08000000U /* Main Flash memory (up to 512 KB) */
#define SRAM1_BASE            0x20000000U /* System SRAM1 (112 KB) */
#define SRAM2_BASE            0x2001C000U /* System SRAM2 (16 KB) */
#define SYSTEM_MEMORY_BASE    0x1FFF0000U /* System Memory (ROM) */
#define OPTION_BYTES_BASE     0x1FFFC000U
#define UID_BASE              0x1FFF7A10U

#define PERIPH_BASE           0x40000000UL /* Peripheral base address */
#define APB1_BASE             (PERIPH_BASE + 0x00000000U)
#define APB2_BASE             (PERIPH_BASE + 0x00010000U)
#define AHB1_BASE             (PERIPH_BASE + 0x00020000U)
#define AHB2_BASE             (PERIPH_BASE + 0x10000000U)
#define AHB3_BASE             0xA0000000UL

#define GPIOA_BASE            (AHB1_BASE + 0x0000U)
#define GPIOB_BASE            (AHB1_BASE + 0x0400U)
#define GPIOC_BASE            (AHB1_BASE + 0x0800U)
#define GPIOD_BASE            (AHB1_BASE + 0x0C00U)
#define GPIOE_BASE            (AHB1_BASE + 0x1000U)
#define GPIOF_BASE            (AHB1_BASE + 0x1400U)
#define GPIOG_BASE            (AHB1_BASE + 0x1800U)
#define GPIOH_BASE            (AHB1_BASE + 0x1C00U)

#define RCC_BASE              (AHB1_BASE + 0x3800UL)
#define FLASH_R_BASE          (AHB1_BASE + 0x3C00UL) /* Flash interface registers (not the memory) */
#define PWR_BASE              (APB1_BASE + 0x7000UL)

/* Cortex-M4 core peripherals */
#define SYSTICK_BASE          0xE000E010UL
#define SCB_SHPR3             (*(volatile uint32_t*)0xE000ED20UL) /* SysTick priority in [31:24] */


#define I2C1_BASE             (APB1_BASE + 0x5400U)
#define I2C2_BASE             (APB1_BASE + 0x5800U)
#define I2C3_BASE             (APB1_BASE + 0x5C00U)

#define SPI2_BASE             (APB1_BASE + 0x3800U)
#define SPI3_BASE             (APB1_BASE + 0x3C00U)

#define USART2_BASE           (APB1_BASE + 0x4400U)
#define USART3_BASE           (APB1_BASE + 0x4800U)

#define UART4_BASE            (APB1_BASE + 0x4C00U)
#define UART5_BASE            (APB1_BASE + 0x5000U)

#define USART1_BASE           (APB2_BASE + 0x1000U)
#define USART6_BASE           (APB2_BASE + 0x1400U)

#define SPI1_BASE             (APB2_BASE + 0x3000U)
#define SPI4_BASE             (APB2_BASE + 0x3400U)
#define SAI1_BASE             (APB2_BASE + 0x5800U)
#define SAI2_BASE             (APB2_BASE + 0x5C00U)

#define EXTI_BASE             (APB2_BASE + 0x3C00U)
#define SYSCFG_BASE           (APB2_BASE + 0x3800U)
#define NVIC_BASE_ADDR        0xE000E100U

/*
 * NVIC register blocks, used as arrays. Use the functions in
 * stm32f446xx_nvic.h instead of touching these directly.
 *   ISER[n] - Set-Enable:   bit k enables   IRQ (32 * n + k), write 1 to act
 *   ICER[n] - Clear-Enable: bit k disables  IRQ (32 * n + k), write 1 to act
 *   IPR[n]  - Priority:     byte k holds the priority of IRQ (4 * n + k)
 */
#define NVIC_ISER_BASE_ADDR     ((volatile uint32_t*)(NVIC_BASE_ADDR + 0x000UL))
#define NVIC_ICER_BASE_ADDR     ((volatile uint32_t*)(NVIC_BASE_ADDR + 0x080UL))
#define NVIC_PR_BASE_ADDR       ((volatile uint32_t*)(NVIC_BASE_ADDR + 0x300UL))

/* The Cortex-M4 in this part implements only the upper 4 bits of each 8-bit
 * priority field; the low 4 bits read as zero. */
#define NO_PR_BITS_IMPLEMENTED  4

/**
 * General Purpose I/O port register structure definition
 */
typedef struct {
    volatile uint32_t MODER;   /* GPIO port mode register,               Address offset: 0x00 */
    volatile uint32_t OTYPER;  /* GPIO port output type register,        Address offset: 0x04 */
    volatile uint32_t OSPEEDR; /* GPIO port output speed register,       Address offset: 0x08 */
    volatile uint32_t PUPDR;   /* GPIO port pull-up/pull-down register,  Address offset: 0x0C */
    volatile uint32_t IDR;     /* GPIO port input data register,         Address offset: 0x10 */
    volatile uint32_t ODR;     /* GPIO port output data register,        Address offset: 0x14 */
    volatile uint32_t BSRR;    /* GPIO port bit set/reset register,      Address offset: 0x18 */
    volatile uint32_t LCKR;    /* GPIO port configuration lock register, Address offset: 0x1C */
    volatile uint32_t AFR[2];  /* GPIO alternate function registers AFR[0] = AFRL; AFR[1] = AFRL,     Address offset: 0x20-0x24 */
} GPIO_RegDef_t;

/**
 * Reset and Clock Control (RCC) register structure definition
 */
typedef struct {
    volatile uint32_t CR;           /* RCC clock control register,                Address offset: 0x00 */
    volatile uint32_t PLLCFGR;      /* RCC PLL configuration register,            Address offset: 0x04 */
    volatile uint32_t CFGR;         /* RCC clock configuration register,          Address offset: 0x08 */
    volatile uint32_t CIR;          /* RCC clock interrupt register,              Address offset: 0x0C */
    volatile uint32_t AHB1RSTR;     /* RCC AHB1 peripheral reset register,        Address offset: 0x10 */
    volatile uint32_t AHB2RSTR;     /* RCC AHB2 peripheral reset register,        Address offset: 0x14 */
    volatile uint32_t AHB3RSTR;     /* RCC AHB3 peripheral reset register,        Address offset: 0x18 */
    uint32_t          RESERVED0;    /* Reserved,                                  Address offset: 0x1C */
    volatile uint32_t APB1RSTR;     /* RCC APB1 peripheral reset register,        Address offset: 0x20 */
    volatile uint32_t APB2RSTR;     /* RCC APB2 peripheral reset register,        Address offset: 0x24 */
    uint32_t          RESERVED1[2]; /* Reserved,                                  Address offset: 0x28-0x2C */
    volatile uint32_t AHB1ENR;      /* RCC AHB1 peripheral clock enable register, Address offset: 0x30 */
    volatile uint32_t AHB2ENR;      /* RCC AHB2 peripheral clock enable register, Address offset: 0x34 */
    volatile uint32_t AHB3ENR;      /* RCC AHB3 peripheral clock enable register, Address offset: 0x38 */
    uint32_t          RESERVED2;    /* Reserved,                                  Address offset: 0x3C */
    volatile uint32_t APB1ENR;      /* RCC APB1 peripheral clock enable register, Address offset: 0x40 */
    volatile uint32_t APB2ENR;      /* RCC APB2 peripheral clock enable register, Address offset: 0x44 */
    uint32_t          RESERVED3[2]; /* Reserved,                                  Address offset: 0x48-0x4C */
    volatile uint32_t AHB1LPENR;    /* RCC AHB1 peripheral clock enable in LP,    Address offset: 0x50 */
    volatile uint32_t AHB2LPENR;    /* RCC AHB2 peripheral clock enable in LP,    Address offset: 0x54 */
    volatile uint32_t AHB3LPENR;    /* RCC AHB3 peripheral clock enable in LP,    Address offset: 0x58 */
    uint32_t          RESERVED4;    /* Reserved,                                  Address offset: 0x5C */
    volatile uint32_t APB1LPENR;    /* RCC APB1 peripheral clock enable in LP,    Address offset: 0x60 */
    volatile uint32_t APB2LPENR;    /* RCC APB2 peripheral clock enable in LP,    Address offset: 0x64 */
    uint32_t          RESERVED5[2]; /* Reserved,                                  Address offset: 0x68-0x6C */
    volatile uint32_t BDCR;         /* RCC Backup domain control register,        Address offset: 0x70 */
    volatile uint32_t CSR;          /* RCC clock control & status register,       Address offset: 0x74 */
    uint32_t          RESERVED6[2]; /* Reserved,                                  Address offset: 0x78-0x7C */
    volatile uint32_t SSCGR;        /* RCC spread spectrum clock gen register,    Address offset: 0x80 */
    volatile uint32_t PLLI2SCFGR;   /* RCC PLLI2S configuration register,         Address offset: 0x84 */
    volatile uint32_t PLLSAICFGR;   /* RCC PLLSAI configuration register,         Address offset: 0x88 */
    volatile uint32_t DCKCFGR;      /* RCC dedicated clock configuration reg 1,   Address offset: 0x8C */
    volatile uint32_t CKGATENR;     /* RCC clocks gated enable register,          Address offset: 0x90 */
    volatile uint32_t DCKCFGR2;     /* RCC dedicated clock configuration reg 2,   Address offset: 0x94 */
} RCC_RegDef_t;

/**
 * Power controller (PWR) register structure definition
 */
typedef struct {
    volatile uint32_t CR;           /* PWR power control register,                Address offset: 0x00 */
    volatile uint32_t CSR;          /* PWR power control/status register,         Address offset: 0x04 */
} PWR_RegDef_t;

/**
 * Flash interface register structure definition
 */
typedef struct {
    volatile uint32_t ACR;          /* Flash access control register,              Address offset: 0x00 */
    volatile uint32_t KEYR;         /* Flash key register,                         Address offset: 0x04 */
    volatile uint32_t OPTKEYR;      /* Flash option key register,                  Address offset: 0x08 */
    volatile uint32_t SR;           /* Flash status register,                      Address offset: 0x0C */
    volatile uint32_t CR;           /* Flash control register,                     Address offset: 0x10 */
    volatile uint32_t OPTCR;        /* Flash option control register,              Address offset: 0x14 */
} FLASH_RegDef_t;

/**
 * SysTick timer (Cortex-M4 core) register structure definition
 */
typedef struct {
    volatile uint32_t CTRL;         /* SysTick control and status register,        Address offset: 0x00 */
    volatile uint32_t LOAD;         /* SysTick reload value register (24 bit),     Address offset: 0x04 */
    volatile uint32_t VAL;          /* SysTick current value register,             Address offset: 0x08 */
    volatile uint32_t CALIB;        /* SysTick calibration value register,         Address offset: 0x0C */
} SYSTICK_RegDef_t;

typedef struct
{
	volatile uint32_t IMR;    /* Interrupt mask register            (Address offset: 0x00) */
	volatile uint32_t EMR;    /* Event mask register                (Address offset: 0x04) */
	volatile uint32_t RTSR;   /* Rising trigger selection register   (Address offset: 0x08) */
	volatile uint32_t FTSR;   /* Falling trigger selection register  (Address offset: 0x0C) */
	volatile uint32_t SWIER;  /* Software interrupt event register  (Address offset: 0x10) */
	volatile uint32_t PR;     /* Pending register                   (Address offset: 0x14) */
} EXTI_RegDef_t;

typedef struct
{
	volatile uint32_t MEMRMP;       /* Memory remap register,              Address offset: 0x00 */
	volatile uint32_t PMC;          /* Peripheral mode configuration,      Address offset: 0x04 */
	volatile uint32_t EXTICR[4];    /* External interrupt configuration,    Address offset: 0x08-0x14 */
  /* Note: Some STM32 variants include CMPCR (Compensation cell) or CFGR registers here */
} SYSCFG_RegDef_t;

typedef struct {
    volatile uint32_t CR1;     /* SPI Control Register 1,        Address offset: 0x00 */
    volatile uint32_t CR2;     /* SPI Control Register 2,        Address offset: 0x04 */
    volatile uint32_t SR;      /* SPI Status Register,           Address offset: 0x08 */
    volatile uint32_t DR;      /* SPI Data Register,             Address offset: 0x0C */
    volatile uint32_t CRCPR;   /* SPI CRC Polynomial Register,   Address offset: 0x10 */
    volatile uint32_t RXCRCR;  /* SPI RX CRC Register,           Address offset: 0x14 */
    volatile uint32_t TXCRCR;  /* SPI TX CRC Register,           Address offset: 0x18 */
    volatile uint32_t I2SCFGR; /* SPI I2S Configuration Register,Address offset: 0x1C */
    volatile uint32_t I2SPR;   /* SPI I2S Prescaler Register,    Address offset: 0x20 */
} SPI_RegDef_t;

/**
 * I2C peripheral register structure definition
 */
typedef struct {
    volatile uint32_t CR1;     /* I2C Control Register 1,        Address offset: 0x00 */
    volatile uint32_t CR2;     /* I2C Control Register 2,        Address offset: 0x04 */
    volatile uint32_t OAR1;    /* I2C Own Address Register 1,    Address offset: 0x08 */
    volatile uint32_t OAR2;    /* I2C Own Address Register 2,    Address offset: 0x0C */
    volatile uint32_t DR;      /* I2C Data Register,             Address offset: 0x10 */
    volatile uint32_t SR1;     /* I2C Status Register 1,         Address offset: 0x14 */
    volatile uint32_t SR2;     /* I2C Status Register 2,         Address offset: 0x18 */
    volatile uint32_t CCR;     /* I2C Clock Control Register,    Address offset: 0x1C */
    volatile uint32_t TRISE;   /* I2C TRISE Register,            Address offset: 0x20 */
    volatile uint32_t FLTR;    /* I2C FLTR Register,             Address offset: 0x24 */
} I2C_RegDef_t;

/**
 * USART / UART peripheral register structure definition
 * (USART1, USART2, USART3, USART6 and UART4, UART5 share this layout)
 */
typedef struct {
    volatile uint32_t SR;      /* USART Status Register,                  Address offset: 0x00 */
    volatile uint32_t DR;      /* USART Data Register,                    Address offset: 0x04 */
    volatile uint32_t BRR;     /* USART Baud Rate Register,               Address offset: 0x08 */
    volatile uint32_t CR1;     /* USART Control Register 1,               Address offset: 0x0C */
    volatile uint32_t CR2;     /* USART Control Register 2,               Address offset: 0x10 */
    volatile uint32_t CR3;     /* USART Control Register 3,               Address offset: 0x14 */
    volatile uint32_t GTPR;    /* USART Guard Time and Prescaler Register,Address offset: 0x18 */
} UART_RegDef_t;

#define GPIOA ((GPIO_RegDef_t *)GPIOA_BASE)
#define GPIOB ((GPIO_RegDef_t *)GPIOB_BASE)
#define GPIOC ((GPIO_RegDef_t *)GPIOC_BASE)
#define GPIOD ((GPIO_RegDef_t *)GPIOD_BASE)
#define GPIOE ((GPIO_RegDef_t *)GPIOE_BASE)
#define GPIOF ((GPIO_RegDef_t *)GPIOF_BASE)
#define GPIOG ((GPIO_RegDef_t *)GPIOG_BASE)
#define GPIOH ((GPIO_RegDef_t *)GPIOH_BASE)

#define SPI1  ((SPI_RegDef_t *)SPI1_BASE)
#define SPI2  ((SPI_RegDef_t *)SPI2_BASE)
#define SPI3  ((SPI_RegDef_t *)SPI3_BASE)
#define SPI4  ((SPI_RegDef_t *)SPI4_BASE)

#define I2C1  ((I2C_RegDef_t *)I2C1_BASE)
#define I2C2  ((I2C_RegDef_t *)I2C2_BASE)
#define I2C3  ((I2C_RegDef_t *)I2C3_BASE)

#define USART1 ((UART_RegDef_t *)USART1_BASE)
#define USART2 ((UART_RegDef_t *)USART2_BASE)
#define USART3 ((UART_RegDef_t *)USART3_BASE)
#define UART4  ((UART_RegDef_t *)UART4_BASE)
#define UART5  ((UART_RegDef_t *)UART5_BASE)
#define USART6 ((UART_RegDef_t *)USART6_BASE)

#define RCC ((RCC_RegDef_t *)RCC_BASE)
#define PWR ((PWR_RegDef_t *)PWR_BASE)
#define FLASH ((FLASH_RegDef_t *)FLASH_R_BASE)
#define SYSTICK ((SYSTICK_RegDef_t *)SYSTICK_BASE)
#define EXTI ((EXTI_RegDef_t *)EXTI_BASE)
#define SYSCFG ((SYSCFG_RegDef_t *)SYSCFG_BASE)

/* ========================================================================== */
/*   GPIO Peripheral Clock Enable Macros                                      */
/* ========================================================================== */

#define GPIOA_PCLK_EN()    (RCC->AHB1ENR |= (1 << 0))  /* Enable GPIOA Clock */
#define GPIOB_PCLK_EN()    (RCC->AHB1ENR |= (1 << 1))  /* Enable GPIOB Clock */
#define GPIOC_PCLK_EN()    (RCC->AHB1ENR |= (1 << 2))  /* Enable GPIOC Clock */
#define GPIOD_PCLK_EN()    (RCC->AHB1ENR |= (1 << 3))  /* Enable GPIOD Clock */
#define GPIOE_PCLK_EN()    (RCC->AHB1ENR |= (1 << 4))  /* Enable GPIOE Clock */
#define GPIOF_PCLK_EN()    (RCC->AHB1ENR |= (1 << 5))  /* Enable GPIOF Clock */
#define GPIOG_PCLK_EN()    (RCC->AHB1ENR |= (1 << 6))  /* Enable GPIOG Clock */
#define GPIOH_PCLK_EN()    (RCC->AHB1ENR |= (1 << 7))  /* Enable GPIOH Clock */

/* ========================================================================== */
/*   GPIO Peripheral Clock Disable Macros                                     */
/* ========================================================================== */

#define GPIOA_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 0)) /* Disable GPIOA Clock */
#define GPIOB_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 1)) /* Disable GPIOB Clock */
#define GPIOC_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 2)) /* Disable GPIOC Clock */
#define GPIOD_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 3)) /* Disable GPIOD Clock */
#define GPIOE_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 4)) /* Disable GPIOE Clock */
#define GPIOF_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 5)) /* Disable GPIOF Clock */
#define GPIOG_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 6)) /* Disable GPIOG Clock */
#define GPIOH_PCLK_DI()    (RCC->AHB1ENR &= ~(1 << 7)) /* Disable GPIOH Clock */

/* ========================================================================== */
/*   GPIO Peripheral peripheral reset register
 * (Pushes the hardware reset signal high, Pulls the reset signal back low)   */
/* ========================================================================== */

#define GPIOA_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 0); RCC->AHB1RSTR &= ~(1 << 0);}while(0)  /* RESET GPIOA REGISTER */
#define GPIOB_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 1); RCC->AHB1RSTR &= ~(1 << 1);}while(0)  /* RESET GPIOB REGISTER */
#define GPIOC_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 2); RCC->AHB1RSTR &= ~(1 << 2);}while(0)  /* RESET GPIOC REGISTER */
#define GPIOD_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 3); RCC->AHB1RSTR &= ~(1 << 3);}while(0)  /* RESET GPIOD REGISTER */
#define GPIOE_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 4); RCC->AHB1RSTR &= ~(1 << 4);}while(0)  /* RESET GPIOE REGISTER */
#define GPIOF_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 5); RCC->AHB1RSTR &= ~(1 << 5);}while(0)  /* RESET GPIOF REGISTER */
#define GPIOG_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 6); RCC->AHB1RSTR &= ~(1 << 6);}while(0)  /* RESET GPIOG REGISTER */
#define GPIOH_REG_RESET()    do{RCC->AHB1RSTR |= (1 << 7); RCC->AHB1RSTR &= ~(1 << 7);}while(0)  /* RESET GPIOH REGISTER */

/* ========================================================================== */
/*   SPI Peripheral Clock Macros                                              */
/* ========================================================================== */
/* SPI1 & SPI4 are on APB2 bus; SPI2 & SPI3 are on APB1 bus */
#define SPI1_PCLK_EN()      (RCC->APB2ENR |= (1 << 12))
#define SPI2_PCLK_EN()      (RCC->APB1ENR |= (1 << 14))
#define SPI3_PCLK_EN()      (RCC->APB1ENR |= (1 << 15))
#define SPI4_PCLK_EN()      (RCC->APB2ENR |= (1 << 13))

#define SPI1_PCLK_DI()      (RCC->APB2ENR &= ~(1 << 12))
#define SPI2_PCLK_DI()      (RCC->APB1ENR &= ~(1 << 14))
#define SPI3_PCLK_DI()      (RCC->APB1ENR &= ~(1 << 15))
#define SPI4_PCLK_DI()      (RCC->APB2ENR &= ~(1 << 13))

/* ========================================================================== */
/*   SPI Peripheral peripheral reset register
 * (Pushes the hardware reset signal high, Pulls the reset signal back low)   */
/* ========================================================================== */

#define SPI1_REG_RESET()    do{RCC->APB2RSTR |= (1 << 12); RCC->APB2RSTR &= ~(1 << 12);}while(0)  /* RESET SPI1 REGISTER */
#define SPI2_REG_RESET()    do{RCC->APB1RSTR |= (1 << 14); RCC->APB1RSTR &= ~(1 << 14);}while(0)  /* RESET SPI2 REGISTER */
#define SPI3_REG_RESET()    do{RCC->APB1RSTR |= (1 << 15); RCC->APB1RSTR &= ~(1 << 15);}while(0)  /* RESET SPI3 REGISTER */
#define SPI4_REG_RESET()    do{RCC->APB2RSTR |= (1 << 13); RCC->APB2RSTR &= ~(1 << 13);}while(0)  /* RESET SPI4 REGISTER */

/* ========================================================================== */
/*   I2C Peripheral Clock Macros                                              */
/* ========================================================================== */
/* I2C1, I2C2, I2C3, and FMPI2C1 are all on the APB1 bus */
#define I2C1_PCLK_EN()      (RCC->APB1ENR |= (1 << 21))
#define I2C2_PCLK_EN()      (RCC->APB1ENR |= (1 << 22))
#define I2C3_PCLK_EN()      (RCC->APB1ENR |= (1 << 23))
#define FMPI2C1_PCLK_EN()   (RCC->APB1ENR |= (1 << 24))

#define I2C1_PCLK_DI()      (RCC->APB1ENR &= ~(1 << 21))
#define I2C2_PCLK_DI()      (RCC->APB1ENR &= ~(1 << 22))
#define I2C3_PCLK_DI()      (RCC->APB1ENR &= ~(1 << 23))
#define FMPI2C1_PCLK_DI()   (RCC->APB1ENR &= ~(1 << 24))

/* ========================================================================== */
/*   I2C Peripheral peripheral reset register
 * (Pushes the hardware reset signal high, Pulls the reset signal back low)   */
/* ========================================================================== */

#define I2C1_REG_RESET()    do{RCC->APB1RSTR |= (1 << 21); RCC->APB1RSTR &= ~(1 << 21);}while(0)  /* RESET I2C1 REGISTER */
#define I2C2_REG_RESET()    do{RCC->APB1RSTR |= (1 << 22); RCC->APB1RSTR &= ~(1 << 22);}while(0)  /* RESET I2C2 REGISTER */
#define I2C3_REG_RESET()    do{RCC->APB1RSTR |= (1 << 23); RCC->APB1RSTR &= ~(1 << 23);}while(0)  /* RESET I2C3 REGISTER */

/* ========================================================================== */
/*   Bit position definitions of I2C peripheral                               */
/* ========================================================================== */

/*
 * I2C CR1 bit positions
 */
#define I2C_CR1_PE                          0  /* Peripheral enable                       */
#define I2C_CR1_SMBUS                       1  /* SMBus mode                              */
#define I2C_CR1_SMBTYPE                     3  /* SMBus type                              */
#define I2C_CR1_ENARP                       4  /* ARP enable                              */
#define I2C_CR1_ENPEC                       5  /* PEC enable                              */
#define I2C_CR1_ENGC                        6  /* General call enable                     */
#define I2C_CR1_NOSTRETCH                   7  /* Clock stretching disable (slave mode)   */
#define I2C_CR1_START                       8  /* Start generation                        */
#define I2C_CR1_STOP                        9  /* Stop generation                         */
#define I2C_CR1_ACK                        10  /* Acknowledge enable                      */
#define I2C_CR1_POS                        11  /* Acknowledge/PEC position (for reception)*/
#define I2C_CR1_PEC                        12  /* Packet error checking                   */
#define I2C_CR1_ALERT                      13  /* SMBus alert                             */
#define I2C_CR1_SWRST                      15  /* Software reset                          */

/*
 * I2C CR2 bit positions
 */
#define I2C_CR2_FREQ                        0  /* Peripheral clock frequency [5:0]        */
#define I2C_CR2_ITERREN                     8  /* Error interrupt enable                  */
#define I2C_CR2_ITEVTEN                     9  /* Event interrupt enable                  */
#define I2C_CR2_ITBUFEN                    10  /* Buffer interrupt enable                 */
#define I2C_CR2_DMAEN                      11  /* DMA requests enable                     */
#define I2C_CR2_LAST                       12  /* DMA last transfer                       */

/*
 * I2C OAR1 bit positions
 */
#define I2C_OAR1_ADD0                       0  /* Interface address bit 0 (10-bit mode)   */
#define I2C_OAR1_ADD71                      1  /* Interface address bits [7:1]            */
#define I2C_OAR1_ADD98                      8  /* Interface address bits [9:8] (10-bit)   */
#define I2C_OAR1_ADDMODE                   15  /* Addressing mode (slave): 0=7bit, 1=10bit*/

/*
 * I2C OAR2 bit positions
 */
#define I2C_OAR2_ENDUAL                     0  /* Dual addressing mode enable             */
#define I2C_OAR2_ADD2                       1  /* Interface address bits [7:1]            */

/*
 * I2C SR1 bit positions
 */
#define I2C_SR1_SB                          0  /* Start bit (master mode)                 */
#define I2C_SR1_ADDR                        1  /* Address sent / matched                  */
#define I2C_SR1_BTF                         2  /* Byte transfer finished                  */
#define I2C_SR1_ADD10                       3  /* 10-bit header sent                      */
#define I2C_SR1_STOPF                       4  /* Stop detection (slave mode)             */
#define I2C_SR1_RXNE                        6  /* Data register not empty (receivers)     */
#define I2C_SR1_TXE                         7  /* Data register empty (transmitters)      */
#define I2C_SR1_BERR                        8  /* Bus error                               */
#define I2C_SR1_ARLO                        9  /* Arbitration lost (master mode)          */
#define I2C_SR1_AF                         10  /* Acknowledge failure                     */
#define I2C_SR1_OVR                        11  /* Overrun / Underrun                      */
#define I2C_SR1_PECERR                     12  /* PEC error in reception                  */
#define I2C_SR1_TIMEOUT                    14  /* Timeout or Tlow error                   */
#define I2C_SR1_SMBALERT                   15  /* SMBus alert                             */

/*
 * I2C SR2 bit positions
 */
#define I2C_SR2_MSL                         0  /* Master/slave                            */
#define I2C_SR2_BUSY                        1  /* Bus busy                                */
#define I2C_SR2_TRA                         2  /* Transmitter/receiver                    */
#define I2C_SR2_GENCALL                     4  /* General call address (slave mode)       */
#define I2C_SR2_SMBDEFAULT                  5  /* SMBus device default address            */
#define I2C_SR2_SMBHOST                     6  /* SMBus host header (slave mode)          */
#define I2C_SR2_DUALF                       7  /* Dual flag (slave mode)                  */
#define I2C_SR2_PEC                         8  /* Packet error checking register [15:8]   */

/*
 * I2C CCR bit positions
 */
#define I2C_CCR_CCR                         0  /* Clock control register [11:0]           */
#define I2C_CCR_DUTY                       14  /* Fast mode duty cycle                    */
#define I2C_CCR_FS                         15  /* I2C master mode selection: 0=Sm, 1=Fm   */

/*
 * I2C FLTR bit positions
 */
#define I2C_FLTR_DNF                        0  /* Digital noise filter [3:0]              */
#define I2C_FLTR_ANOFF                      4  /* Analog noise filter OFF                 */

/* ========================================================================== */
/*   USART / UART Peripheral Clock Macros                                     */
/* ========================================================================== */
/* USART1 & USART6 are on APB2 bus; USART2, USART3, UART4, UART5 are on APB1 */
#define USART1_PCLK_EN()    (RCC->APB2ENR |= (1 << 4))
#define USART2_PCLK_EN()    (RCC->APB1ENR |= (1 << 17))
#define USART3_PCLK_EN()    (RCC->APB1ENR |= (1 << 18))
#define UART4_PCLK_EN()     (RCC->APB1ENR |= (1 << 19))
#define UART5_PCLK_EN()     (RCC->APB1ENR |= (1 << 20))
#define USART6_PCLK_EN()    (RCC->APB2ENR |= (1 << 5))

#define USART1_PCLK_DI()    (RCC->APB2ENR &= ~(1 << 4))
#define USART2_PCLK_DI()    (RCC->APB1ENR &= ~(1 << 17))
#define USART3_PCLK_DI()    (RCC->APB1ENR &= ~(1 << 18))
#define UART4_PCLK_DI()     (RCC->APB1ENR &= ~(1 << 19))
#define UART5_PCLK_DI()     (RCC->APB1ENR &= ~(1 << 20))
#define USART6_PCLK_DI()    (RCC->APB2ENR &= ~(1 << 5))

/* ========================================================================== */
/*   USART / UART Peripheral peripheral reset register
 * (Pushes the hardware reset signal high, Pulls the reset signal back low)   */
/* ========================================================================== */

#define USART1_REG_RESET()  do{RCC->APB2RSTR |= (1 << 4);  RCC->APB2RSTR &= ~(1 << 4);}while(0)   /* RESET USART1 REGISTER */
#define USART2_REG_RESET()  do{RCC->APB1RSTR |= (1 << 17); RCC->APB1RSTR &= ~(1 << 17);}while(0)  /* RESET USART2 REGISTER */
#define USART3_REG_RESET()  do{RCC->APB1RSTR |= (1 << 18); RCC->APB1RSTR &= ~(1 << 18);}while(0)  /* RESET USART3 REGISTER */
#define UART4_REG_RESET()   do{RCC->APB1RSTR |= (1 << 19); RCC->APB1RSTR &= ~(1 << 19);}while(0)  /* RESET UART4 REGISTER  */
#define UART5_REG_RESET()   do{RCC->APB1RSTR |= (1 << 20); RCC->APB1RSTR &= ~(1 << 20);}while(0)  /* RESET UART5 REGISTER  */
#define USART6_REG_RESET()  do{RCC->APB2RSTR |= (1 << 5);  RCC->APB2RSTR &= ~(1 << 5);}while(0)   /* RESET USART6 REGISTER */

/* ========================================================================== */
/*   Bit position definitions of USART / UART peripheral (RM0390 section 25.6) */
/* ========================================================================== */

/*
 * USART SR bit positions
 */
#define USART_SR_PE                         0  /* Parity error                            */
#define USART_SR_FE                         1  /* Framing error                           */
#define USART_SR_NF                         2  /* Noise detected flag                     */
#define USART_SR_ORE                        3  /* Overrun error                           */
#define USART_SR_IDLE                       4  /* IDLE line detected                      */
#define USART_SR_RXNE                       5  /* Read data register not empty            */
#define USART_SR_TC                         6  /* Transmission complete                   */
#define USART_SR_TXE                        7  /* Transmit data register empty            */
#define USART_SR_LBD                        8  /* LIN break detection flag                */
#define USART_SR_CTS                        9  /* CTS flag (not available on UART4/5)     */

/*
 * USART BRR bit positions
 */
#define USART_BRR_DIV_FRACTION              0  /* Fraction of USARTDIV [3:0]              */
#define USART_BRR_DIV_MANTISSA              4  /* Mantissa of USARTDIV [15:4]             */

/*
 * USART CR1 bit positions
 */
#define USART_CR1_SBK                       0  /* Send break                              */
#define USART_CR1_RWU                       1  /* Receiver wakeup                         */
#define USART_CR1_RE                        2  /* Receiver enable                         */
#define USART_CR1_TE                        3  /* Transmitter enable                      */
#define USART_CR1_IDLEIE                    4  /* IDLE interrupt enable                   */
#define USART_CR1_RXNEIE                    5  /* RXNE interrupt enable                   */
#define USART_CR1_TCIE                      6  /* Transmission complete interrupt enable  */
#define USART_CR1_TXEIE                     7  /* TXE interrupt enable                    */
#define USART_CR1_PEIE                      8  /* PE interrupt enable                     */
#define USART_CR1_PS                        9  /* Parity selection: 0=even, 1=odd         */
#define USART_CR1_PCE                      10  /* Parity control enable                   */
#define USART_CR1_WAKE                     11  /* Wakeup method                           */
#define USART_CR1_M                        12  /* Word length: 0=8 data bits, 1=9         */
#define USART_CR1_UE                       13  /* USART enable                            */
#define USART_CR1_OVER8                    15  /* Oversampling mode: 0=by 16, 1=by 8      */

/*
 * USART CR2 bit positions
 */
#define USART_CR2_ADD                       0  /* Address of the USART node [3:0]         */
#define USART_CR2_LBDL                      5  /* LIN break detection length              */
#define USART_CR2_LBDIE                     6  /* LIN break detection interrupt enable    */
#define USART_CR2_LBCL                      8  /* Last bit clock pulse                    */
#define USART_CR2_CPHA                      9  /* Clock phase (synchronous mode)          */
#define USART_CR2_CPOL                     10  /* Clock polarity (synchronous mode)       */
#define USART_CR2_CLKEN                    11  /* Clock enable (synchronous mode)         */
#define USART_CR2_STOP                     12  /* STOP bits [13:12]                       */
#define USART_CR2_LINEN                    14  /* LIN mode enable                         */

/*
 * USART CR3 bit positions
 */
#define USART_CR3_EIE                       0  /* Error interrupt enable (FE, ORE, NF)    */
#define USART_CR3_IREN                      1  /* IrDA mode enable                        */
#define USART_CR3_IRLP                      2  /* IrDA low-power                          */
#define USART_CR3_HDSEL                     3  /* Half-duplex selection                   */
#define USART_CR3_NACK                      4  /* Smartcard NACK enable                   */
#define USART_CR3_SCEN                      5  /* Smartcard mode enable                   */
#define USART_CR3_DMAR                      6  /* DMA enable receiver                     */
#define USART_CR3_DMAT                      7  /* DMA enable transmitter                  */
#define USART_CR3_RTSE                      8  /* RTS enable                              */
#define USART_CR3_CTSE                      9  /* CTS enable                              */
#define USART_CR3_CTSIE                    10  /* CTS interrupt enable                    */
#define USART_CR3_ONEBIT                   11  /* One sample bit method enable            */

/* ========================================================================== */
/*   PWR: clock macros and bit positions                                      */
/* ========================================================================== */
#define PWR_PCLK_EN()       (RCC->APB1ENR |= (1 << 28))
#define PWR_PCLK_DI()       (RCC->APB1ENR &= ~(1 << 28))

#define PWR_CR_VOS                         14  /* Regulator voltage scaling [15:14], 11 = scale 1 */
#define PWR_CR_ODEN                        16  /* Over-drive enable                       */
#define PWR_CR_ODSWEN                      17  /* Over-drive switching enable             */

#define PWR_CSR_VOSRDY                     14  /* Voltage scaling ready                   */
#define PWR_CSR_ODRDY                      16  /* Over-drive mode ready                   */
#define PWR_CSR_ODSWRDY                    17  /* Over-drive switching ready              */

/* ========================================================================== */
/*   FLASH interface bit positions                                            */
/* ========================================================================== */
#define FLASH_ACR_LATENCY                   0  /* Wait states [3:0]                       */
#define FLASH_ACR_PRFTEN                    8  /* Prefetch enable                         */
#define FLASH_ACR_ICEN                      9  /* Instruction cache enable                */
#define FLASH_ACR_DCEN                     10  /* Data cache enable                       */

/* ========================================================================== */
/*   SYSCFG Peripheral Clock Macros                                           */
/* ========================================================================== */
/* SYSCFG is on the APB2 bus */
#define SYSCFG_PCLK_EN()    (RCC->APB2ENR |= (1 << 14))
#define SYSCFG_PCLK_DI()    (RCC->APB2ENR &= ~(1 << 14))

/**
 * On/off argument of every XXX_PeriClockControl, XXX_PeripheralControl and
 * XXX_IRQInterruptConfig function
 */
typedef enum {
    DRV_DISABLE = 0,
    DRV_ENABLE  = 1
} DRV_State_t;

/**
 * Result of every blocking transfer and of every interrupt transfer start
 */
typedef enum {
    DRV_OK      = 0,   /* done (blocking) or accepted (interrupt start)            */
    DRV_ERROR   = 1,   /* bad argument, or the other side refused (I2C NACK)       */
    DRV_BUSY    = 2,   /* an interrupt transfer is still running on this handle    */
    DRV_TIMEOUT = 3    /* the hardware did not answer within the Timeout           */
} DRV_Status_t;

/* Timeout value that means "wait forever" */
#define DRV_MAX_DELAY      0xFFFFFFFFU

#define IRQ_NO_EXTI0      6
#define IRQ_NO_EXTI1      7
#define IRQ_NO_EXTI2      8
#define IRQ_NO_EXTI3      9
#define IRQ_NO_EXTI4     10
#define IRQ_NO_EXTI9_5   23
#define IRQ_NO_EXTI15_10 40
#define IRQ_NO_SPI1      35
#define IRQ_NO_SPI2      36
#define IRQ_NO_SPI3      51
#define IRQ_NO_SPI4      84
#define IRQ_NO_I2C1_EV   31
#define IRQ_NO_I2C1_ER   32
#define IRQ_NO_I2C2_EV   33
#define IRQ_NO_I2C2_ER   34
#define IRQ_NO_I2C3_EV   72
#define IRQ_NO_I2C3_ER   73
#define IRQ_NO_USART1    37
#define IRQ_NO_USART2    38
#define IRQ_NO_USART3    39
#define IRQ_NO_UART4     52
#define IRQ_NO_UART5     53
#define IRQ_NO_USART6    71

/* Macros for all the possible NVIC priority levels */
#define NVIC_IRQ_PRI0       0U  /* Absolute Highest Priority */
#define NVIC_IRQ_PRI1       1U
#define NVIC_IRQ_PRI2       2U
#define NVIC_IRQ_PRI3       3U
#define NVIC_IRQ_PRI4       4U
#define NVIC_IRQ_PRI5       5U
#define NVIC_IRQ_PRI6       6U
#define NVIC_IRQ_PRI7       7U
#define NVIC_IRQ_PRI8       8U
#define NVIC_IRQ_PRI9       9U
#define NVIC_IRQ_PRI10      10U
#define NVIC_IRQ_PRI11      11U
#define NVIC_IRQ_PRI12      12U
#define NVIC_IRQ_PRI13      13U
#define NVIC_IRQ_PRI14      14U
#define NVIC_IRQ_PRI15      15U /* Absolute Lowest Priority */

#include "stm32f446xx_rcc.h"
#include "stm32f446xx_systick.h"
#include "stm32f446xx_nvic.h"
#include "stm32f446xx_gpio.h"
#include "stm32f446xx_spi.h"
#include "stm32f446xx_i2c.h"
#include "stm32f446xx_uart.h"

#endif /* INC_STM32F446XX_H_ */
