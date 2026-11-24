/* D168UV overrides for the shared D890UV cfw sources (shared/ = D890UV_src/cfw). Included by shared hal/hal.h and
 * app/cfwcp.h through __has_include, so the D890UV build (which has no board_cfg.h) is unchanged. */
#ifndef CFW_BOARD_CFG_H
#define CFW_BOARD_CFG_H
#include <stdint.h>
#include <stdbool.h>
#include "at32f403a.h"

#define CFW_BOARD_D168UV   1
#define CFW_BOARD_NAME     "D168UV"
#define CFW_RF_CHIPS       1
#define CFW_TXT_MODEKEY    "PF1: change mode"
#define CFW_TXT_SOFT_MEM   "PF2:MEM"
#define CFW_TXT_SOFT_VFO   "PF2:VFO"
#define CFW_TXT_DAC        "PWM PB9 "
#define CFW_KEY_SLOT_NAMES "PF1", "PF2", NULL, NULL, NULL, NULL   /* side keys post KEY_P1 / KEY_P2, no PF3 */
#define CFW_ADC_TEMP_SLOT  2               /* ADC1 ch12 (PC2), meaning unknown */

/* GPIO helpers, F1 register layout (SCR = BSRR at 0x10, IDT at 0x08) */
#define CFW_BOARD_GPIO_INLINE 1
static inline void gpio_set(uint32_t port, int pin) { *(volatile uint32_t *)(port + 0x10) = 1u << pin; }
static inline void gpio_clr(uint32_t port, int pin) { *(volatile uint32_t *)(port + 0x14) = 1u << pin; }
static inline void gpio_write(uint32_t port, int pin, bool v) { if(v) { gpio_set(port, pin); } else { gpio_clr(port, pin); } }
static inline bool gpio_read(uint32_t port, int pin) { return (*(volatile uint32_t *)(port + 0x08) >> pin) & 1; }

/* LCD: same controller as the D890UV but a portrait panel (stock V1.07 screen is 128 wide, 160 high) */
#define LCD_W              128
#define LCD_H              160

/* RAM: 224 KB (vs 768 KB on the D890UV) */
#define ADC_FRAME_CH       3u              /* ch10 PC0 = RX audio (stock decoder input), ch11 PC1 = battery, ch12 PC2 */
#define ADC_RING_FRAMES    3840u           /* 80 ms at 48 kHz, as the D890UV */
#define DAC_RING           3840u
/* TX audio = TMR4_CH4 PWM on PB9, period 2500 (48 kHz): ring holds CCR4 values */
#define DAC_CODE(c)        ((uint16_t)(((uint32_t)(c) * 2500u + 2048u) >> 12))
#define CFW_MCU_FM_TX      1               /* analog FM test tone through PB9 allowed on every build */
#define CFW_DBG_SYM_SIZE   2048
#define CFW_GFX_ROWHASH    1               /* no 40 KB front buffer */
#define CFW_NAND_CODEPLUG  0               /* stock D168 NAND channel layout not decoded: cfw codeplug or built-ins */

/* emulator-only symbol FIFO (EMU=1 builds, emu/models/SymbolSource.cs): 0x5FFF0500 is the D168 I2C model */
#define SYMS_BASE          0x5FFF0600u

/* internal flash (1 MB, 2 KB sectors): codeplug 0x080A0000-0x080FFFFF (bank 2), see docs/CODEPLUG.md */
#define CFWCP_BASE         0x080A0000u
#define CFWCP_SIZE         0x00060000u     /* 384 KB, same as the D890UV */
#define CFWCP_PAGE         0x2000u         /* logical erase unit = 4 sectors */
#define CFWCP_FIRST_PAGE   0u
#define CFWCP_MODEL_ID     1u              /* header u16 at 18: only D168UV codeplugs are accepted */
#define CFW_FLASH_EXTERNAL 1
void cfw_flash_unlock(void);
void cfw_flash_lock(void);
void cfw_flash_erase(uint32_t addr);                         /* one CFWCP_PAGE (4 sectors), codeplug range only */
void cfw_flash_program16(uint32_t addr, const uint32_t w[4]); /* 16 bytes, codeplug range only */

/* CPS link on USART1 (PA9 TX / PA10 RX, the programming jack) */
#define CFW_PROG_UART_EXTERNAL 1
#define CFW_PROG_MODEL     'D', '1', '6', '8'                /* ident "D168CFW" */
void prog_uart_hw_init(void);
void prog_uart_hw_txkick(void);
void prog_uart_rx_byte(uint8_t b);
int  prog_uart_tx_next(void);

/* VOX: the radio's own detector (hal.h vox_hw_*), no MCU mic input (stock reads none either) */
#endif
