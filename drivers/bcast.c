/* D168UV FM broadcast receiver (app/bcast.h): I2C 0x10, RDA5807-style. The stock firmware V1.07 writes registers
 * 0x02..0x05 as one 8-byte sequential write (FUN_0803a77c) from a RAM image at 0x200013E5 (boot value
 * 80 00 8B 9A 04 03 85 AF, read in the emulator):
 *   tune  FUN_08078078(f / 10 kHz): reg 02 = 0xC009 (on, unmuted), reg 03 = ((f / 10 kHz - 7600) / 5) << 6 | 0x1A
 *         (channel in 50 kHz steps from 76 MHz, tune, band 76-108 MHz, 50 kHz spacing), reg 04 low byte = 2,
 *         reg 05 low nibble = 0xF (volume)
 *   off   FUN_08078058: reg 02 = 0x8000, reg 04 low byte = 3
 * No AM on this radio. Unverified on hardware. */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"
#include "bcast.h"

bool i2c_write(uint8_t a7, const uint8_t *d, int n);

#define RDA 0x10

static uint8_t s_img[8] = { 0x80, 0x00, 0x8B, 0x9A, 0x04, 0x03, 0x85, 0xAF };
static int s_band;
static uint32_t s_hz, s_writes;
static bool s_ack = true;
static uint8_t s_vol = 15;

static void put_img(void) { s_ack = i2c_write(RDA, s_img, 8); s_writes++; }

bool bcast_has_am(void) { return false; }
int bcast_band(void) { return s_band; }
uint32_t bcast_hz(void) { return s_hz; }
bool bcast_ack(void) { return s_ack; }
uint32_t bcast_writes(void) { return s_writes; }

bool bcast_on(int band, uint32_t hz)
{
    if(band != BC_FM) { if(band == BC_OFF) { bcast_off(); return true; } return false; }
    if(hz < 76000000u || hz > BC_FM_MAX) { return false; }
    uint32_t ch = ((hz / 10000u - 7600u) / 5u & 0x3FFu) << 6;
    s_img[0] = 0xC0; s_img[1] = 0x09;
    s_img[2] = (uint8_t)(ch >> 8); s_img[3] = (uint8_t)(ch | 0x1A);
    s_img[5] = 2;
    s_img[7] = (uint8_t)((s_img[7] & 0xF0) | (s_vol & 0x0F));
    put_img();
    s_band = BC_FM;
    s_hz = hz;
    audio_route(AUDIO_RF);
    return s_ack;
}

void bcast_off(void)
{
    if(s_band == BC_OFF) { return; }
    s_img[0] = 0x80; s_img[1] = 0x00; s_img[5] = 3;
    put_img();
    s_band = BC_OFF;
    s_hz = 0;
}

void bcast_volume(uint8_t v)
{
    s_vol = v > 15 ? 15 : v;
    if(s_band == BC_FM) { s_img[7] = (uint8_t)((s_img[7] & 0xF0) | s_vol); s_img[3] &= (uint8_t)~0x10; put_img(); }
}
