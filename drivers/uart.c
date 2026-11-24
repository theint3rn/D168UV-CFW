/* USART1 (programming jack: PA9 TX, PA10 RX, stock IRQ37_Handler) for the shared CPS link app/prog.c.
 * 115200 8N1 from PCLK2 60 MHz. Receive and transmit run from the interrupt; prog.c owns the rings.
 * Note: PA10 doubles as the speaker-mic PTT input on this radio (stock FUN_08092f08 reads it while PB8 is low). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

#define BAUD 115200u
#define PCLK2_HZ 60000000u

void USART1_IRQHandler(void)
{
    uint32_t sts = USART1_STS;
    if(sts & ((1u << 5) | (1u << 3))) {                 /* RDBF or overrun: reading DT clears both */
        prog_uart_rx_byte((uint8_t)USART1_DT);
    }
    if((USART1_CTRL1 & (1u << 7)) && (USART1_STS & (1u << 7))) {    /* TDBEIEN + TDBE */
        int b = prog_uart_tx_next();
        if(b < 0) { USART1_CTRL1 &= ~(1u << 7); }
        else { USART1_DT = (uint32_t)b; }
    }
}

void prog_uart_hw_init(void)
{
    CRM_APB2EN |= CRM_APB2_USART1 | CRM_APB2_GPIOA;
    (void)CRM_APB2EN;
    gpio_af(GPIOA, 9, 0);
    gpio_mode(GPIOA, 10, GPIO_IN, PULL_UP);
    USART1_CTRL1 = 0;
    USART1_BAUDR = (PCLK2_HZ + BAUD / 2u) / BAUD;
    USART1_CTRL2 = 0;
    USART1_CTRL3 = 0;
    USART1_CTRL1 = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);   /* UEN, RDBFIEN, TEN, REN */
    nvic_enable(IRQ_USART1);
}

void prog_uart_hw_txkick(void)
{
    uint32_t p = irq_save();
    USART1_CTRL1 |= 1u << 7;                           /* TDBEIEN */
    irq_restore(p);
}

/* USART2 = GPS receiver (stock: NMEA, IRQ38 -> FUN_0803a11c), 9600 8N1 receive only, from PCLK1 60 MHz. The RX pin is
 * left as the stock firmware / reset set it (not mapped yet). Bytes go to the shared NMEA parser (app/gps.c). */
#define USART2_BASE 0x40004400u
void gps_uart_byte(uint8_t b);

void USART2_IRQHandler(void)
{
    uint32_t sts = REG32(USART2_BASE + 0x00);
    if(sts & ((1u << 5) | (1u << 3))) { gps_uart_byte((uint8_t)REG32(USART2_BASE + 0x04)); }
}

void gps_uart_hw_init(void)
{
    CRM_APB1EN |= 1u << 17;                            /* USART2 */
    CRM_APB2EN |= CRM_APB2_GPIOA;
    (void)CRM_APB1EN;
    /* pins as the stock V1.07 leaves them after boot (emulator GPIO dump, no USART2 remap): PA2 = TX (AF push-pull),
     * PA3 = RX (input, pull-down: ODR bit 3 = 0). The GPS module's power switch is not mapped: it is left as the stock firmware set it. */
    gpio_mode(GPIOA, 2, GPIO_AF, PULL_NONE);
    gpio_mode(GPIOA, 3, GPIO_IN, PULL_DOWN);
    REG32(USART2_BASE + 0x0C) = 0;
    REG32(USART2_BASE + 0x08) = (60000000u + 4800u) / 9600u;
    REG32(USART2_BASE + 0x0C) = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);   /* UEN, RDBFIEN, TEN, REN */
    nvic_enable(38);
}

/* GPS module set-up as stock V1.07 sends it on USART2 TX (emulator usart2.txt; once at boot and again when GPS is
 * switched on): GNSS system h00000001, then NMEA output enables: GGA (0) and RMC (4) on, the rest off. No GPS power
 * switch found: toggling Menu > GPS > On/Off in the stock emulator changes no output pin (dump of GPIOA-E
 * CRL/CRH/ODR before / after; only the pull bits of the input pins PD6 / PD11 moved once, not repeatably), only these
 * lines and whether the NMEA data is used. */
static void tx_str(const char *p)
{
    for(; *p; p++) {
        for(uint32_t n = 0; n < 200000u && !(REG32(USART2_BASE + 0x00) & (1u << 7)); n++) { }   /* TDBE */
        REG32(USART2_BASE + 0x04) = (uint8_t)*p;
    }
}

void gps_uart_send_cfg(void)
{
    static const uint8_t on[10] = { 1, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
    char b[20] = "$CFGMSG,0,x,y\r\n";
    tx_str("$CFGSYS,h00000001\r\n");
    for(int i = 0; i < 10; i++) { b[10] = (char)('0' + i); b[12] = (char)('0' + on[i]); tx_str(b); }
}
