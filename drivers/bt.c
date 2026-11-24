/* D168UV Bluetooth module (JieLi) on USART4 (0x40004C00, IRQ 52), AT text commands as the stock firmware V1.07:
 *   sender FUN_0800fd30 "AT+" cmd CR LF, command + wait FUN_0800fe3c (5 tries x 250 ms); boot probe FUN_08005b94:
 *   VER? (answer = module present), AGHF?, PIN?, DNAME?, CONN?, SPKEN=0; gains FUN_0801058c "SPKGAIN=" + 2 digits
 *   {11 16 21 26 31} (0x0801C4F6) and "MICGAIN=" {06 10 14 18 22} (0x0801C500), index = Optional Settings
 *   BhtSpkGain / BhtMicGain.
 * The cfw asks the module only when Bluetooth is On in the codeplug; it sets the gains and shows version / name /
 * connection (Menu > Radio extras > Bluetooth). Not done: audio over Bluetooth, BT PTT, pairing.
 * Pins: the module's UART pins are not traced in the stock code (USART4 default mapping PC10 TX / PC11 RX, unused by
 * the rest of the board); unverified on hardware. Emulator: models/Bt168.cs. */
#include <string.h>
#include "at32f403a.h"
#include "board.h"
#include "hal.h"
#include "stockfx.h"

#define U4(o) REG32(0x40004C00u + (o))
static bt_status_t s_st;
static char s_rx[40];
static volatile uint8_t s_n, s_line;
static uint8_t s_step, s_try;
static uint32_t s_t;

void USART4_IRQHandler(void)
{
    uint32_t sts = U4(0x00);
    if(!(sts & ((1u << 5) | (1u << 3)))) { return; }
    char c = (char)U4(0x04);
    if(s_line) { return; }                                   /* previous line not taken yet */
    if(c == '\n') { s_rx[s_n] = 0; s_line = s_n > 0; s_n = 0; }
    else if(c != '\r' && s_n < sizeof s_rx - 1) { s_rx[s_n++] = c; }
}

static void put(const char *t)
{
    for(; *t; t++) {
        for(uint32_t k = 0; k < 100000u && !(U4(0x00) & (1u << 7)); k++) { }
        U4(0x04) = (uint8_t)*t;
    }
}
static void cmd(const char *c) { put("AT+"); put(c); put("\r\n"); s_t = hal_millis(); }

static void cmd_gain(const char *name, const char *tab, uint8_t i)
{
    char b[16];
    size_t n = strlen(name);
    memcpy(b, name, n);
    b[n] = tab[2 * i]; b[n + 1] = tab[2 * i + 1]; b[n + 2] = 0;
    cmd(b);
}

void bt_config(bool on, uint8_t mic, uint8_t spk)
{
    memset(&s_st, 0, sizeof s_st);
    s_st.on = on; s_st.mic = mic < 5 ? mic : 2; s_st.spk = spk < 5 ? spk : 2;     /* out of range = 2 as stock */
    s_step = on ? 1 : 0; s_try = 0;
    if(!on) { return; }
    CRM_APB1EN |= 1u << 19;                                   /* USART4 */
    (void)CRM_APB1EN;
    gpio_af(GPIOC, 10, 0);
    gpio_mode(GPIOC, 11, GPIO_IN, PULL_UP);
    U4(0x0C) = 0;
    U4(0x08) = (60000000u + 57600u) / 115200u;
    U4(0x10) = 0; U4(0x14) = 0;
    U4(0x0C) = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);
    nvic_enable(52);
}

static void take(void)
{
    if(!s_line) { return; }
    if(strncmp(s_rx, "+VER:", 5) == 0) { strncpy(s_st.ver, s_rx + 5, sizeof s_st.ver - 1); s_st.present = true; }
    else if(strncmp(s_rx, "+DNAME:", 7) == 0) { strncpy(s_st.name, s_rx + 7, sizeof s_st.name - 1); }
    else if(strncmp(s_rx, "+CONN:", 6) == 0) { s_st.connected = strncmp(s_rx + 6, "000000000000", 12) != 0; }
    s_line = 0;
}

void bt_tick(void)
{
    take();
    if(!s_step || s_step > 8) { return; }
    uint32_t now = hal_millis();
    if(s_step > 1 || s_try) { if(now - s_t < 250u) { return; } }
    switch(s_step) {
    case 1:                                                   /* probe: 5 tries */
        if(s_st.present) { s_step = 2; return; }
        if(s_try++ >= 5) { s_step = 9; return; }
        cmd("VER?");
        return;
    case 2: cmd("AGHF?"); break;
    case 3: cmd("PIN?"); break;
    case 4: cmd("DNAME?"); break;
    case 5: cmd("CONN?"); break;
    case 6: cmd("SPKEN=0"); break;
    case 7: cmd_gain("MICGAIN=", "0610141822", s_st.mic); break;
    case 8: cmd_gain("SPKGAIN=", "1116212631", s_st.spk); break;
    }
    s_step++;
}

const bt_status_t *bt_status(void) { return &s_st; }
