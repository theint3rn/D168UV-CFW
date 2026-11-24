/* D168UV board pin map (AT32F403A). Source: stock firmware V1.07 (decomp/src/fw_*.c, functions cited per line;
 * GPIO helpers there: FUN_080143e0 = set (SCR), FUN_08013ffa = clear (CLR), FUN_08014548 = read (IDT)) and the
 * GPIO configuration the stock firmware leaves after boot in the emulator (emu run "cfwprobe"). */
#ifndef CFW_BOARD_H
#define CFW_BOARD_H
#include <stdbool.h>
#include <stdint.h>
#include "at32f403a.h"

/* Power: PC3 = latch (set at the end of the stock init FUN_08015594, cleared by the power-off paths
 * FUN_080043d8 / FUN_0800c48c). PA0 = volume-knob switch: high = off (stock FUN_0800c48c powers off;
 * high at boot = charge-only loop FUN_08005444). PC13 = charger present (low, FUN_0800b6ac). */
#define PIN_PWR_LATCH_PORT GPIOC
#define PIN_PWR_LATCH      3
#define PIN_PWR_KEY_PORT   GPIOA
#define PIN_PWR_KEY        0
#define PIN_CHG_PORT       GPIOC
#define PIN_CHG            13

/* LCD 8080 bus, same wiring as the D890UV (data PE7..PE14 = also key rows) */
#define LCD_DATA_PORT      GPIOE
#define LCD_DATA_SHIFT     7
#define LCD_CTRL_PORT      GPIOD
#define LCD_DC             12      /* PD12 D/C# */
#define LCD_RST            13      /* PD13 reset (stock FUN_0803d2fc pulses it) */
#define LCD_CS             14      /* PD14 CS# */
#define LCD_WR             15      /* PD15 WR# */

/* Key matrix (stock FUN_0804339c): rows PE7..PE12 driven low one at a time, columns PC5 / PB1 / PB2 active low */
#define KEY_ROWS           6
#define KEY_COL0_PORT      GPIOC
#define KEY_COL0           5
#define KEY_COL1_PORT      GPIOB
#define KEY_COL1           1
#define KEY_COL2_PORT      GPIOB
#define KEY_COL2           2

/* Side keys (stock FUN_08092f08): PTT PD0, PF1 PA13 (bit 4), PF2 PA14 (bit 8), active low.
 * PA13/PA14 = SWDIO/SWCLK: using them as keys disables SWD (stock does the same; make KEEP_SWD=1 keeps it).
 * PA10 = speaker-mic PTT (shares USART1 RX), only while PB8 (accessory detect) reads low. */
#define PIN_PTT_PORT       GPIOD
#define PIN_PTT            0
#define PIN_PF1_PORT       GPIOA
#define PIN_PF1            13
#define PIN_PF2_PORT       GPIOA
#define PIN_PF2            14
#define PIN_EXTPTT_PORT    GPIOA
#define PIN_EXTPTT         10
#define PIN_ACC_DET_PORT   GPIOB
#define PIN_ACC_DET        8

/* Channel encoder: PC8 / PC9 on EXTI8 / EXTI9 (IRQ23 EXTI9_5, stock IRQ23_Handler 0x0800679c -> FUN_0800b5f0) */
#define ENC_PORT           GPIOC
#define ENC_A              8
#define ENC_B              9

/* SPI-NAND (Kioxia 2 Gbit, 2 KB pages), bit-banged (stock FUN_080072bc write, FUN_08006e08 / FUN_08006f50 read):
 * CS# PE2, SCK PB11, IO0/MOSI PB10, IO1/MISO PA15 (stock x2 read: PA15 = high bit, PB10 = low bit). */
#define NAND_CS_PORT       GPIOE
#define NAND_CS            2
#define NAND_SCK_PORT      GPIOB
#define NAND_SCK           11
#define NAND_IO0_PORT      GPIOB
#define NAND_IO0           10
#define NAND_IO1_PORT      GPIOA
#define NAND_IO1           15
#define NAND_PAGE          2048u

/* RF transceiver (same chip as D890UV rf0), 3-wire: SCK PB3, SDA PB4, CS# PB5 (stock FUN_0807bc60 write,
 * FUN_0807bca8 read). One chip only. */
#define RF_SCK_PORT        GPIOB
#define RF_SCK             3
#define RF_SDA_PORT        GPIOB
#define RF_SDA             4
#define RF_CS0_PORT        GPIOB
#define RF_CS0             5

/* TX lines: EXTI5 handler (stock IRQ23_Handler, PD5 = baseband TX-slot signal) switches PE6 and PC4 together with
 * the APC level on DAC1: PD5 low -> PE6 low, PC4 low, APC 0; PD5 high -> PC4 high, PE6 high, APC level. */
#define TX_EN0_PORT        GPIOE
#define TX_EN0             6
#define TX_EN1_PORT        GPIOC
#define TX_EN1             4
#define BB_TXSLOT_PORT     GPIOD
#define BB_TXSLOT          5

/* SCT3288 baseband on SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI), CS# PB12, data ready PD11, frame pending PD6
 * (stock FUN_0800fc5c / IRQ36_Handler 0x0808a204: MCU reads by clocking 0xA5). PD7..PD10 are driven by the stock
 * baseband power-up FUN_0800c558 (PD8/PD9/PD10 low then PD7, PD8 high); PD9 pulse = reset (FUN_0800e4d0). */
#define BB_CS_PORT         GPIOB
#define BB_CS              12
#define BB_RDY_PORT        GPIOD
#define BB_RDY             11
#define BB_PEND            6
#define BB_RST             9
#define BB_PWR             7

/* Analog: ADC1 ch10 PC0 = RX audio / discriminator (confirmed by stock: the TMR6 IRQ FUN_08010000 feeds PC0
 * samples into the stock tone / AFSK decoder FUN_08011000; coupling - flat or de-emphasised - not known), ch11 PC1
 * battery (stock FUN_080326b4: V*100 = (raw*0x528>>12) + cal - 100), ch12 PC2 (unknown). Stock scan FUN_0803c8a4,
 * DMA1 ch1 -> 0x200017cc. DAC1 PA4 = APC level (stock FUN_08004364), DAC2 PA5 = RX tune voltage by frequency
 * (stock FUN_0809099c / FUN_0800cdf8, drivers/dac.c). */
#define ADC_CH_DISC        10
#define ADC_CH_BATT        11
#define ADC_CH_AUX         12

/* TX audio from the MCU: PB9 = TMR4_CH4 PWM (stock FUN_08091bd4 / FUN_08095c70), PD1 = mic path (low while the
 * MCU drives TX audio, stock FUN_08009430 / FUN_0800940c). PWM period 2500 timer clocks = 48 kHz. */
#define TX_PWM_PORT        GPIOB
#define TX_PWM             9
#define TX_PWM_TOP         2500u
#define TX_MIC_PORT        GPIOD
#define TX_MIC             1

/* Audio routing (stock FUN_08070a70(mode)): 0/2 = PE0 low, PC12 low (muted); 1 = PE0 high (RF-chip audio, analog
 * modes); 3 = PC12 high (baseband / MCU tone audio: voice prompts, beeps, digital voice). PA7 = speaker amplifier:
 * set in every mode except when stock flags (settings +0xb3 == 0, DAT_20000a50 == 1, DAT_20000a7e == 0) clear it;
 * cleared when a tone ends (FUN_08026b60). Polarity of PA7 not verified (assumed high = amp on). */
#define AUD_RF_PORT        GPIOE
#define AUD_RF             0
#define AUD_BB_PORT        GPIOC
#define AUD_BB             12
#define AUD_AMP_PORT       GPIOA
#define AUD_AMP            7
enum { AUDIO_MUTE = 0, AUDIO_RF = 1, AUDIO_BB = 3 };
void audio_route(int mode);

/* drivers/dac.c: PA5 tune voltage */
bool     dac_tune_load_cal(void);          /* stock NAND calibration block -> tables (false: none, PA5 mid-scale) */
bool     dac_tune_cal_ok(void);
uint16_t dac_tune_value(uint32_t hz);
void     dac_rx_tune(uint32_t hz);

/* Programming jack: USART1 PA9 TX / PA10 RX (stock IRQ37_Handler). */
#endif
