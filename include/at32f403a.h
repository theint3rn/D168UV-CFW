/* Minimal Artery AT32F403A register map for the D168UV custom firmware (STM32F1-style peripherals).
 * Only what the cfw drivers use. Names follow the AT32 reference manual where they differ from ST's. */
#ifndef CFW_AT32F403A_H
#define CFW_AT32F403A_H
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))

/* ---- Cortex-M4F system ---- */
#define SCB_VTOR      REG32(0xE000ED08)
#define SCB_AIRCR     REG32(0xE000ED0C)
#define SCB_SCR       REG32(0xE000ED10)
#define SCB_CPACR     REG32(0xE000ED88)
#define SYST_CSR      REG32(0xE000E010)
#define SYST_RVR      REG32(0xE000E014)
#define SYST_CVR      REG32(0xE000E018)
#define NVIC_ISER(n)  REG32(0xE000E100 + 4 * (n))
#define NVIC_ICER(n)  REG32(0xE000E180 + 4 * (n))
#define NVIC_ICPR(n)  REG32(0xE000E280 + 4 * (n))
#define NVIC_IPR8(n)  REG8(0xE000E400 + (n))

static inline void nvic_enable(int irq)  { NVIC_ICPR(irq >> 5) = 1u << (irq & 31); NVIC_ISER(irq >> 5) = 1u << (irq & 31); }
static inline void nvic_disable(int irq) { NVIC_ICER(irq >> 5) = 1u << (irq & 31); }
static inline uint32_t irq_save(void) { uint32_t p; __asm volatile("mrs %0, primask; cpsid i" : "=r"(p) :: "memory"); return p; }
static inline void irq_restore(uint32_t p) { __asm volatile("msr primask, %0" :: "r"(p) : "memory"); }
static inline void wfi(void) { __asm volatile("dsb; wfi" ::: "memory"); }
static inline void dsb(void) { __asm volatile("dsb" ::: "memory"); }
static inline void isb(void) { __asm volatile("isb" ::: "memory"); }

/* IRQ numbers (AT32F403A = STM32F10x high-density numbering) */
#define IRQ_EXTI0        6
#define IRQ_DMA1_CH1     11
#define IRQ_EXTI9_5      23
#define IRQ_TMR3         29
#define IRQ_SPI2         36
#define IRQ_USART1       37
#define IRQ_EXTI15_10    40
#define IRQ_DMA2_CH4_5   59

/* ---- CRM (clock and reset) @0x40021000 ---- */
#define CRM_BASE      0x40021000u
#define CRM_CTRL      REG32(CRM_BASE + 0x00)
#define CRM_CFG       REG32(CRM_BASE + 0x04)
#define CRM_AHBEN     REG32(CRM_BASE + 0x14)
#define CRM_APB2EN    REG32(CRM_BASE + 0x18)
#define CRM_APB1EN    REG32(CRM_BASE + 0x1C)
#define CRM_MISC2     REG32(CRM_BASE + 0x54)
#define CRM_APB2_AFIO   (1u << 0)
#define CRM_APB2_GPIOA  (1u << 2)
#define CRM_APB2_ADC1   (1u << 9)
#define CRM_APB2_USART1 (1u << 14)
#define CRM_APB1_TMR2   (1u << 0)
#define CRM_APB1_TMR3   (1u << 1)
#define CRM_APB1_TMR4   (1u << 2)
#define CRM_APB1_SPI2   (1u << 14)
#define CRM_APB1_DAC    (1u << 29)
#define CRM_AHB_DMA1    (1u << 0)
#define CRM_AHB_DMA2    (1u << 1)

/* ---- flash controller @0x40022000: bank 1 (0x08000000-0x0807FFFF) / bank 2 (+0x40, 0x08080000-0x080FFFFF) ---- */
#define FLASH_R_BASE  0x40022000u
#define FLASH_PSR     REG32(FLASH_R_BASE + 0x00)
#define FLASH_UNLOCK(b) REG32(FLASH_R_BASE + 0x04 + 0x40 * (b))
#define FLASH_STS(b)    REG32(FLASH_R_BASE + 0x0C + 0x40 * (b))
#define FLASH_CTRL(b)   REG32(FLASH_R_BASE + 0x10 + 0x40 * (b))
#define FLASH_ADDR(b)   REG32(FLASH_R_BASE + 0x14 + 0x40 * (b))
#define FLASH_STS_OBF   (1u << 0)
#define FLASH_CTRL_FPRGM (1u << 0)
#define FLASH_CTRL_SECERS (1u << 1)
#define FLASH_CTRL_ERSTR (1u << 6)
#define FLASH_CTRL_OPLK  (1u << 7)
#define FLASH_KEY1    0x45670123u
#define FLASH_KEY2    0xCDEF89ABu
#define FLASH_SECTOR  0x800u                     /* 2 KB sectors */

/* ---- GPIO (F1 style): CFGLR/CFGHR 4 bits per pin, IDT, ODT, SCR (set/reset), CLR ---- */
#define GPIOA         0x40010800u
#define GPIOB         0x40010C00u
#define GPIOC         0x40011000u
#define GPIOD         0x40011400u
#define GPIOE         0x40011800u
#define GPIO_CFGLR(p) REG32((p) + 0x00)
#define GPIO_CFGHR(p) REG32((p) + 0x04)
#define GPIO_IDT(p)   REG32((p) + 0x08)
#define GPIO_ODT(p)   REG32((p) + 0x0C)
#define GPIO_BSRR(p)  REG32((p) + 0x10)          /* = AT32 SCR: low half sets, high half resets */
#define GPIO_BRR(p)   REG32((p) + 0x14)          /* = AT32 CLR */

/* ---- AFIO (IOMUX) @0x40010000 ---- */
#define AFIO_BASE     0x40010000u
#define AFIO_MAP      REG32(AFIO_BASE + 0x04)    /* SWJTAG_MUX [26:24] */
#define AFIO_EXTIC(n) REG32(AFIO_BASE + 0x08 + 4 * (n))   /* n = 0..3, 4 bits per line */

/* ---- EXTI @0x40010400 ---- */
#define EXTI_BASE     0x40010400u
#define EXTI_INTEN    REG32(EXTI_BASE + 0x00)
#define EXTI_POLCFG1  REG32(EXTI_BASE + 0x08)    /* rising */
#define EXTI_POLCFG2  REG32(EXTI_BASE + 0x0C)    /* falling */
#define EXTI_INTSTS   REG32(EXTI_BASE + 0x14)    /* write 1 to clear */

/* ---- timers (TMR2 / TMR3 on APB1, timer clock 120 MHz) ---- */
#define TMR2_BASE     0x40000000u
#define TMR3_BASE     0x40000400u
#define TMR4_BASE     0x40000800u
#define TMR_CTRL1(t)  REG32((t) + 0x00)
#define TMR_CTRL2(t)  REG32((t) + 0x04)
#define TMR_IDEN(t)   REG32((t) + 0x0C)          /* DIER: OVFDEN (update DMA) = bit 8 */
#define TMR_SWEVT(t)  REG32((t) + 0x14)
#define TMR_CM2(t)    REG32((t) + 0x1C)          /* CCMR2: ch4 = bits 15:8 */
#define TMR_CCTRL(t)  REG32((t) + 0x20)          /* CCER: C4EN = bit 12 */
#define TMR_C4DT(t)   REG32((t) + 0x40)          /* CCR4 */
#define TMR_C4DT_ADDR(t) ((t) + 0x40)
#define TMR_DIV(t)    REG32((t) + 0x28)
#define TMR_PR(t)     REG32((t) + 0x2C)

/* ---- ADC1 @0x40012400 ---- */
#define ADC1_BASE     0x40012400u
#define ADC1_STS      REG32(ADC1_BASE + 0x00)
#define ADC1_CTRL1    REG32(ADC1_BASE + 0x04)
#define ADC1_CTRL2    REG32(ADC1_BASE + 0x08)
#define ADC1_SPT1     REG32(ADC1_BASE + 0x0C)    /* ch10..17 */
#define ADC1_SPT2     REG32(ADC1_BASE + 0x10)    /* ch0..9 */
#define ADC1_OSQ1     REG32(ADC1_BASE + 0x2C)
#define ADC1_OSQ2     REG32(ADC1_BASE + 0x30)
#define ADC1_OSQ3     REG32(ADC1_BASE + 0x34)
#define ADC1_ODT_ADDR (ADC1_BASE + 0x4C)

/* ---- DAC @0x40007400 ---- */
#define DAC_BASE      0x40007400u
#define DAC_CTRL      REG32(DAC_BASE + 0x00)
#define DAC_D1DTH12R  REG32(DAC_BASE + 0x08)
#define DAC_D2DTH12R  REG32(DAC_BASE + 0x14)
#define DAC_D2DTH12R_ADDR (DAC_BASE + 0x14)

/* ---- DMA (F1 layout): channel n = 1..7 ---- */
#define DMA1_BASE     0x40020000u
#define DMA2_BASE     0x40020400u
#define DMA_ISR(d)    REG32((d) + 0x00)
#define DMA_IFCR(d)   REG32((d) + 0x04)
#define DMA_CCR(d,n)  REG32((d) + 0x08 + 0x14 * ((n) - 1))
#define DMA_CNDTR(d,n) REG32((d) + 0x0C + 0x14 * ((n) - 1))
#define DMA_CPAR(d,n) REG32((d) + 0x10 + 0x14 * ((n) - 1))
#define DMA_CMAR(d,n) REG32((d) + 0x14 + 0x14 * ((n) - 1))

/* ---- SPI2 @0x40003800 ---- */
#define SPI2_BASE     0x40003800u
#define SPI2_CTRL1    REG32(SPI2_BASE + 0x00)
#define SPI2_CTRL2    REG32(SPI2_BASE + 0x04)
#define SPI2_STS      REG32(SPI2_BASE + 0x08)
#define SPI2_DT       REG32(SPI2_BASE + 0x0C)

/* ---- USART1 @0x40013800 ---- */
#define USART1_BASE   0x40013800u
#define USART1_STS    REG32(USART1_BASE + 0x00)
#define USART1_DT     REG32(USART1_BASE + 0x04)
#define USART1_BAUDR  REG32(USART1_BASE + 0x08)
#define USART1_CTRL1  REG32(USART1_BASE + 0x0C)
#define USART1_CTRL2  REG32(USART1_BASE + 0x10)
#define USART1_CTRL3  REG32(USART1_BASE + 0x14)

#endif
