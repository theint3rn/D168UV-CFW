/* Channel knob: quadrature encoder PC8 (A) / PC9 (B), both edges on EXTI8 / EXTI9 = IRQ23 (EXTI9_5).
 * Stock IRQ23_Handler 0x0800679c samples PC8 / PC9 per edge, FUN_0800b5f0 decodes. Here: Gray-code table,
 * one detent = two valid transitions in the same direction (same as the D890UV cfw). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

static volatile int32_t s_count;
static volatile uint8_t s_state;
static volatile int8_t s_acc;

static const int8_t k_step[16] = {
    0, -1, +1, 0,
    +1, 0, 0, -1,
    -1, 0, 0, +1,
    0, +1, -1, 0,
};

static void sample(void)
{
    uint8_t a = gpio_read(ENC_PORT, ENC_A), b = gpio_read(ENC_PORT, ENC_B);
    uint8_t now = (uint8_t)((a << 1) | b);
    int8_t d = k_step[(s_state << 2) | now];
    s_state = now;
    if(!d) { return; }
    if((s_acc > 0 && d < 0) || (s_acc < 0 && d > 0)) { s_acc = 0; }
    s_acc += d;
    if(s_acc >= 2) { s_acc = 0; s_count++; keys_post(KEY_KNOB_CW); }
    else if(s_acc <= -2) { s_acc = 0; s_count--; keys_post(KEY_KNOB_CCW); }
}

void EXTI9_5_IRQHandler(void)
{
    uint32_t pend = EXTI_INTSTS & ((1u << ENC_A) | (1u << ENC_B));
    EXTI_INTSTS = pend ? pend : (0x1Fu << 5);
    sample();
}

void encoder_init(void)
{
    s_state = (uint8_t)((gpio_read(ENC_PORT, ENC_A) << 1) | gpio_read(ENC_PORT, ENC_B));
    /* EXTIC3 (lines 8..11): line 8 bits 3:0, line 9 bits 7:4 = port C (2) */
    AFIO_EXTIC(2) = (AFIO_EXTIC(2) & ~0xFFu) | (2u << 0) | (2u << 4);
    EXTI_POLCFG1 |= (1u << ENC_A) | (1u << ENC_B);
    EXTI_POLCFG2 |= (1u << ENC_A) | (1u << ENC_B);
    EXTI_INTSTS = (1u << ENC_A) | (1u << ENC_B);
    EXTI_INTEN |= (1u << ENC_A) | (1u << ENC_B);
    NVIC_IPR8(IRQ_EXTI9_5) = 0x80;
    nvic_enable(IRQ_EXTI9_5);
}

int32_t encoder_count(void) { return s_count; }
