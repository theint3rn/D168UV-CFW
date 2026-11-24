/* ADC1: 3-channel scan at 48 kHz (TMR3 TRGO) into a circular DMA1 channel 1 ring.
 * Stock FUN_0803c8a4 scans ch10 (PC0), ch11 (PC1, battery), ch12 (PC2) continuously into 3 halfwords at
 * 0x200017cc (DMA1 ch1). The cfw keeps the channels, triggers each scan from TMR3 at 48 kHz and keeps
 * ADC_RING_FRAMES scans, because ch10 is taken as the receiver audio / discriminator for the MCU-DSP demods
 * (ASSUMED: stock samples PC0 from TMR6 FUN_08010000; the real discriminator tap is unconfirmed).
 * Frame k = ring[3k .. 3k+2] = ch10, ch11 (battery), ch12. Write position = DMA1 CNDTR1 (no interrupt).
 *   ADC clock PCLK2/4 = 15 MHz, sample time 13.5 cycles: 3 x 26 cycles = 5.2 us per scan (< 20.8 us).
 *   TMR3: 120 MHz, DIV 0, PR 2499 -> 48 kHz, TRGO = update; ADC1 OCTESEL 100 = TMR3 TRGO.
 * Battery = stock FUN_080326b4: V*100 = (raw*0x528 >> 12) + cal - 100 (cal from NAND in stock; 100 here). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

static volatile uint16_t s_ring[ADC_RING_FRAMES * ADC_FRAME_CH] __attribute__((aligned(4)));
static uint8_t s_cal = 100;
#define N_HW (ADC_RING_FRAMES * ADC_FRAME_CH)

void battery_init(void)
{
    CRM_APB2EN |= CRM_APB2_ADC1;
    CRM_AHBEN |= CRM_AHB_DMA1;
    CRM_APB1EN |= CRM_APB1_TMR3;
    (void)CRM_APB1EN;
    gpio_mode(GPIOC, 0, GPIO_ANALOG, PULL_NONE);
    gpio_mode(GPIOC, 1, GPIO_ANALOG, PULL_NONE);
    gpio_mode(GPIOC, 2, GPIO_ANALOG, PULL_NONE);

    ADC1_CTRL2 = 1u;                                   /* ADCEN (power up) */
    delay_us(20);
    ADC1_CTRL2 |= 1u << 3;                             /* ADCALINIT */
    for(uint32_t n = 0; n < 100000u && (ADC1_CTRL2 & (1u << 3)); n++) { }
    ADC1_CTRL2 |= 1u << 2;                             /* ADCAL */
    for(uint32_t n = 0; n < 100000u && (ADC1_CTRL2 & (1u << 2)); n++) { }
    ADC1_CTRL1 = 1u << 8;                              /* SQEN: scan */
    ADC1_SPT1 = (2u << 0) | (2u << 3) | (2u << 6);     /* ch10..12: 13.5 cycles */
    ADC1_OSQ1 = (ADC_FRAME_CH - 1u) << 20;             /* 3 conversions */
    ADC1_OSQ2 = 0;
    ADC1_OSQ3 = ADC_CH_DISC | (ADC_CH_BATT << 5) | (ADC_CH_AUX << 10);

    DMA_CCR(DMA1_BASE, 1) = 0;
    DMA_CPAR(DMA1_BASE, 1) = ADC1_ODT_ADDR;
    DMA_CMAR(DMA1_BASE, 1) = (uint32_t)(uintptr_t)s_ring;
    DMA_CNDTR(DMA1_BASE, 1) = N_HW;
    DMA_CCR(DMA1_BASE, 1) = (2u << 12) | (1u << 10) | (1u << 8) | (1u << 7) | (1u << 5) | 1u;  /* PL high, 16/16, MINC, CIRC, EN */

    /* ordinary channels: DMA, external trigger TMR3 TRGO (OCTESEL 100), trigger enable */
    ADC1_CTRL2 = 1u | (1u << 8) | (4u << 17) | (1u << 20);

    TMR_CTRL1(TMR3_BASE) = 0;
    TMR_DIV(TMR3_BASE) = 0;
    TMR_PR(TMR3_BASE) = 120000000u / ADC_FS - 1u;
    TMR_CTRL2(TMR3_BASE) = 2u << 4;                    /* PTOS = update -> TRGO */
    TMR_SWEVT(TMR3_BASE) = 1u;
    TMR_CTRL1(TMR3_BASE) = 1u;                         /* TMREN */
}

const volatile uint16_t *adc_ring(void) { return s_ring; }

uint32_t adc_write_frame(void)
{
    uint32_t left = DMA_CNDTR(DMA1_BASE, 1) & 0xFFFFu;
    if(left == 0 || left > N_HW) { return 0; }
    return ((N_HW - left) / ADC_FRAME_CH) % ADC_RING_FRAMES;
}

uint16_t battery_raw(int i) { return (i >= 0 && i < (int)ADC_FRAME_CH) ? s_ring[i] : 0; }

uint16_t battery_centivolts(void)
{
    uint32_t raw = s_ring[1];
    int v = (int)((raw * 0x528u) >> 12) + s_cal - 100;
    return v < 0 ? 0 : (uint16_t)v;
}
