/* Key matrix + side keys of the D168UV.
 * Matrix = stock FUN_0804339c: drive one row PE7..PE12 low (others high), read the active-low columns
 * PC5 / PB1 / PB2; the table gives the stock key code for (row, column). Codes are the D890UV ones (hal.h).
 * The rows are the LCD data lines, so a scan only runs while the LCD bus is idle.
 * Side keys = stock FUN_08092f08: PTT PD0, PF1 PA13, PF2 PA14 (active low); speaker-mic PTT PA10 while the
 * accessory detect PB8 reads low. PF1 posts KEY_P1 (shared UI: change mode), PF2 KEY_P2 (VFO / memory). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

static const uint8_t k_matrix[KEY_ROWS][3] = {
    /* col:   PC5        PB1        PB2   */
    { KEY_UP,   0,         KEY_DOWN },   /* PE7  */
    { KEY_MENU, KEY_EXIT,  0 },          /* PE8  */
    { KEY_1,    KEY_3,     KEY_2 },      /* PE9  */
    { KEY_4,    KEY_6,     KEY_5 },      /* PE10 */
    { KEY_7,    KEY_9,     KEY_8 },      /* PE11 */
    { KEY_STAR, KEY_HASH,  KEY_0 },      /* PE12 */
};

#define QLEN 16
static volatile uint8_t q[QLEN];
static volatile uint8_t q_head, q_tail;

void keys_post(int ev)
{
    uint32_t p = irq_save();
    uint8_t n = (uint8_t)((q_head + 1) % QLEN);
    if(n != q_tail) { q[q_head] = (uint8_t)ev; q_head = n; }
    irq_restore(p);
}

int keys_get(void)
{
    uint32_t p = irq_save();
    int ev = KEY_NONE;
    if(q_tail != q_head) { ev = q[q_tail]; q_tail = (uint8_t)((q_tail + 1) % QLEN); }
    irq_restore(p);
    return ev;
}

void keys_init(void) { q_head = q_tail = 0; }

uint8_t keys_matrix_raw(void)
{
    if(lcd_busy()) { return 0; }
    uint8_t found = 0;
    for(int r = 0; r < KEY_ROWS && !found; r++) {
        uint32_t row = 1u << (LCD_DATA_SHIFT + r);
        uint32_t all = 0xFFu << LCD_DATA_SHIFT;
        GPIO_BSRR(LCD_DATA_PORT) = (all & ~row) | (row << 16);
        delay_us(20);                                    /* stock: FUN_08041440(100) settle */
        bool c0 = !gpio_read(KEY_COL0_PORT, KEY_COL0);
        bool c1 = !gpio_read(KEY_COL1_PORT, KEY_COL1);
        bool c2 = !gpio_read(KEY_COL2_PORT, KEY_COL2);
        if(c0 && k_matrix[r][0]) { found = k_matrix[r][0]; }
        else if(c1 && k_matrix[r][1]) { found = k_matrix[r][1]; }
        else if(c2 && k_matrix[r][2]) { found = k_matrix[r][2]; }
    }
    GPIO_BSRR(LCD_DATA_PORT) = 0xFFu << LCD_DATA_SHIFT;  /* rows idle high */
    return found;
}

bool key_ptt_down(void)
{
    if(!gpio_read(PIN_PTT_PORT, PIN_PTT)) { return true; }
#ifdef CFW_PROG_HW
    return false;   /* HW=1 HW_SAVE=1: PA10 is the CPS link's RX on a real radio; serial data must never read as jack PTT */
#else
    return !gpio_read(PIN_ACC_DET_PORT, PIN_ACC_DET) && !gpio_read(PIN_EXTPTT_PORT, PIN_EXTPTT);
#endif
}

static uint8_t side_raw(void)
{
    uint8_t b = 0;
    if(key_ptt_down()) { b |= 1; }
#ifndef CFW_KEEP_SWD
    if(!gpio_read(PIN_PF1_PORT, PIN_PF1)) { b |= 2; }
    if(!gpio_read(PIN_PF2_PORT, PIN_PF2)) { b |= 4; }
#endif
    return b;
}

void keys_poll(void)
{
    static uint8_t m_last, m_stable, m_cnt;
    static uint32_t m_since;
    static bool m_long;
    static uint8_t s_last, s_stable;
    static const uint8_t side_code[3] = { KEY_PTT, KEY_P1, KEY_P2 };

    uint8_t m = keys_matrix_raw();
    if(m != m_last) { m_last = m; m_cnt = 0; }
    else if(m_cnt < 2 && ++m_cnt == 2 && m != m_stable) {   /* stable for 2 polls */
        if(m_stable) { keys_post(m_stable | KEY_RELEASED); }
        m_stable = m;
        m_long = false;
        m_since = hal_millis();
        if(m) { keys_post(m); }
    }
    if(m_stable && !m_long && hal_millis() - m_since > 1000) {
        m_long = true;
        keys_post(m_stable | KEY_LONG);
    }

    uint8_t s = side_raw();
    if(s == s_last && s != s_stable) {
        uint8_t ch = s ^ s_stable;
        for(int i = 0; i < 3; i++) {
            if(ch & (1u << i)) { keys_post(side_code[i] | ((s & (1u << i)) ? 0 : KEY_RELEASED)); }
        }
        s_stable = s;
    }
    s_last = s;
}
