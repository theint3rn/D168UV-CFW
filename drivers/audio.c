/* D168UV audio routing, as stock FUN_08070a70(mode) (see hal/board.h): PE0 = RF-chip audio, PC12 = baseband / MCU
 * tone audio, PA7 = speaker amplifier (polarity assumed: high = on). The cfw selects RF audio for analog FM receive,
 * baseband audio for the digital modes and mutes while transmitting (drivers/rf.c). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

static bool s_init, s_tone;
static int s_mode;

void audio_route(int mode)
{
    s_mode = mode;
    if(s_tone) { return; }                       /* a beep is playing: applied when it ends */
    if(!s_init) {
        gpio_mode(AUD_RF_PORT, AUD_RF, GPIO_OUT, PULL_NONE);
        gpio_mode(AUD_BB_PORT, AUD_BB, GPIO_OUT, PULL_NONE);
        gpio_mode(AUD_AMP_PORT, AUD_AMP, GPIO_OUT, PULL_NONE);
        s_init = true;
    }
    if(mode == AUDIO_RF) { gpio_clr(AUD_BB_PORT, AUD_BB); gpio_set(AUD_RF_PORT, AUD_RF); }        /* stock order */
    else if(mode == AUDIO_BB) { gpio_set(AUD_BB_PORT, AUD_BB); gpio_clr(AUD_RF_PORT, AUD_RF); }
    else { gpio_clr(AUD_RF_PORT, AUD_RF); gpio_clr(AUD_BB_PORT, AUD_BB); }
    gpio_write(AUD_AMP_PORT, AUD_AMP, mode != AUDIO_MUTE);
}

/* beeps (app/spk.c): MCU tone audio = PC12 source + amplifier on, RF audio off; then back to the last route */
void audio_tone(bool on)
{
    if(on == s_tone) { return; }
    if(on) {
        int keep = s_mode;
        audio_route(AUDIO_BB);
        s_mode = keep;
        s_tone = true;
    } else {
        s_tone = false;
        audio_route(s_mode);
    }
}

bool audio_tone_muted(void) { return false; }     /* the D168UV volume knob is analog (PC2 = display level only) */
void audio_poll(void) { }

/* Volume limit (Settings > Vol Level, stock settings +0x3B: 0 = Indoor, 1..8 = Level 1..8). Stock V1.07 FUN_0800e958
 * sends it to the SCT3288 baseband, not to an MCU pin: attenuation table 0x0801C688 = {10 0E 0C 0A 08 06 04 02 00}
 * [level] as frame B0 0x40 ID 0x08 (group 2 field 0x0D in FUN_080854dc), and FUN_080113c8 sends B0 0x00 ID 0x69 =
 * {01 51} for Indoor, {1F 40} otherwise (stock RAM 0x2000CC90). Frames seen in the stock emulator's baseband.log when
 * the menu value changes: "84 A9 61 00 03 00 69 01 51 00" + "84 A9 61 00 02 40 08 10" (Level 8 -> Indoor),
 * "00 69 1F 40" + "40 08 0E" (Indoor -> Level 1). The analog volume knob stays in front of it. */
void audio_volume_limit(uint8_t maxvol)
{
    static const uint8_t att[9] = { 0x10, 0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00 };
    static const uint8_t indoor[2] = { 0x01, 0x51 }, normal[2] = { 0x1F, 0x40 };
    if(maxvol > 8) { maxvol = 8; }
    (void)bb_send(0x00, 0x69, maxvol == 0 ? indoor : normal, 2);
    (void)bb_send(0x40, 0x08, &att[maxvol], 1);
}
