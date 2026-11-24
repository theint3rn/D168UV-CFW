/* GPIO set-up for every pin the cfw uses (AT32 / F1 style: 4 configuration bits per pin, pull via ODT). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

void gpio_mode(uint32_t port, int pin, int mode, int pull)
{
    uint32_t cfg;
    switch(mode) {
    case GPIO_OUT:    cfg = 0x3u; break;                       /* push-pull, 50 MHz */
    case GPIO_AF:     cfg = 0xBu; break;                       /* alternate push-pull, 50 MHz */
    case GPIO_ANALOG: cfg = 0x0u; break;
    default:          cfg = pull == PULL_NONE ? 0x4u : 0x8u; break;   /* floating / pull up-down */
    }
    uint32_t p = irq_save();
    if(mode == GPIO_IN && pull != PULL_NONE) {
        if(pull == PULL_UP) { gpio_set(port, pin); } else { gpio_clr(port, pin); }
    }
    volatile uint32_t *r = pin < 8 ? &GPIO_CFGLR(port) : &GPIO_CFGHR(port);
    int s = (pin & 7) * 4;
    *r = (*r & ~(0xFu << s)) | (cfg << s);
    irq_restore(p);
}

void gpio_af(uint32_t port, int pin, int af)
{
    (void)af;                                                  /* F1 style: fixed alternate functions */
    gpio_mode(port, pin, GPIO_AF, PULL_NONE);
}

void gpio_init_board(void)
{
    CRM_APB2EN |= CRM_APB2_AFIO | (0x1Fu << 2);                /* AFIO, GPIOA..GPIOE */
    (void)CRM_APB2EN;
    /* JTAG off (PA15 = NAND MISO, PB3/PB4 = RF bus); SWD off too unless KEEP_SWD (PA13/PA14 = side keys) */
#ifdef CFW_KEEP_SWD
    AFIO_MAP = (AFIO_MAP & ~(7u << 24)) | (2u << 24);
#else
    AFIO_MAP = (AFIO_MAP & ~(7u << 24)) | (4u << 24);
#endif

    gpio_set(PIN_PWR_LATCH_PORT, PIN_PWR_LATCH);               /* keep the radio on */
    gpio_mode(PIN_PWR_LATCH_PORT, PIN_PWR_LATCH, GPIO_OUT, PULL_NONE);
    gpio_mode(PIN_PWR_KEY_PORT, PIN_PWR_KEY, GPIO_IN, PULL_DOWN);
    gpio_mode(PIN_CHG_PORT, PIN_CHG, GPIO_IN, PULL_UP);

    /* LCD bus: data PE7..14, control PD12..15 idle high */
    for(int i = 0; i < 8; i++) {
        gpio_set(LCD_DATA_PORT, LCD_DATA_SHIFT + i);
        gpio_mode(LCD_DATA_PORT, LCD_DATA_SHIFT + i, GPIO_OUT, PULL_NONE);
    }
    GPIO_BSRR(LCD_CTRL_PORT) = (1u << LCD_DC) | (1u << LCD_RST) | (1u << LCD_CS) | (1u << LCD_WR);
    gpio_mode(LCD_CTRL_PORT, LCD_DC, GPIO_OUT, PULL_NONE);
    gpio_mode(LCD_CTRL_PORT, LCD_RST, GPIO_OUT, PULL_NONE);
    gpio_mode(LCD_CTRL_PORT, LCD_CS, GPIO_OUT, PULL_NONE);
    gpio_mode(LCD_CTRL_PORT, LCD_WR, GPIO_OUT, PULL_NONE);

    /* keys */
    gpio_mode(KEY_COL0_PORT, KEY_COL0, GPIO_IN, PULL_UP);
    gpio_mode(KEY_COL1_PORT, KEY_COL1, GPIO_IN, PULL_UP);
    gpio_mode(KEY_COL2_PORT, KEY_COL2, GPIO_IN, PULL_UP);
    gpio_mode(PIN_PTT_PORT, PIN_PTT, GPIO_IN, PULL_UP);
#ifndef CFW_KEEP_SWD
    gpio_mode(PIN_PF1_PORT, PIN_PF1, GPIO_IN, PULL_UP);
    gpio_mode(PIN_PF2_PORT, PIN_PF2, GPIO_IN, PULL_UP);
#endif
    gpio_mode(PIN_ACC_DET_PORT, PIN_ACC_DET, GPIO_IN, PULL_UP);

    /* encoder */
    gpio_mode(ENC_PORT, ENC_A, GPIO_IN, PULL_UP);
    gpio_mode(ENC_PORT, ENC_B, GPIO_IN, PULL_UP);

    /* SPI-NAND: CS high, SCK low, IO0 out, IO1 in */
    gpio_set(NAND_CS_PORT, NAND_CS);
    gpio_mode(NAND_CS_PORT, NAND_CS, GPIO_OUT, PULL_NONE);
    gpio_clr(NAND_SCK_PORT, NAND_SCK);
    gpio_mode(NAND_SCK_PORT, NAND_SCK, GPIO_OUT, PULL_NONE);
    gpio_mode(NAND_IO0_PORT, NAND_IO0, GPIO_OUT, PULL_NONE);
    gpio_mode(NAND_IO1_PORT, NAND_IO1, GPIO_IN, PULL_UP);

    /* RF 3-wire: CS high, SCK low, SDA out low */
    gpio_set(RF_CS0_PORT, RF_CS0);
    gpio_mode(RF_CS0_PORT, RF_CS0, GPIO_OUT, PULL_NONE);
    gpio_clr(RF_SCK_PORT, RF_SCK);
    gpio_mode(RF_SCK_PORT, RF_SCK, GPIO_OUT, PULL_NONE);
    gpio_clr(RF_SDA_PORT, RF_SDA);
    gpio_mode(RF_SDA_PORT, RF_SDA, GPIO_OUT, PULL_NONE);

    /* TX enables low (receive) */
    gpio_clr(TX_EN0_PORT, TX_EN0);
    gpio_mode(TX_EN0_PORT, TX_EN0, GPIO_OUT, PULL_NONE);
    gpio_clr(TX_EN1_PORT, TX_EN1);
    gpio_mode(TX_EN1_PORT, TX_EN1, GPIO_OUT, PULL_NONE);

    /* baseband: CS high, ready / pending in, power PD7, reset PD8 (driven in bb_init) */
    gpio_set(BB_CS_PORT, BB_CS);
    gpio_mode(BB_CS_PORT, BB_CS, GPIO_OUT, PULL_NONE);
    gpio_mode(BB_RDY_PORT, BB_RDY, GPIO_IN, PULL_DOWN);
    gpio_mode(BB_RDY_PORT, BB_PEND, GPIO_IN, PULL_DOWN);
}
