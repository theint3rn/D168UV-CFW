# D168UV audio / modulation paths (from stock V1.07, decompile `../../decomp/src`)

Found 2026-11-22 from the stock firmware. The analog side of these pins (filters, coupling) still needs a
scope on real hardware.

## RX: discriminator / RX audio -> PC0 (ADC1_IN10)
* ADC1 setup FUN_0803c8a4: PC0/PC1/PC2 analog (mask 7), regular scan ch10 (PC0, rank 1), ch11 (PC1, rank 2),
  ch12 (PC2, rank 3, longer sample time), continuous, DMA1 ch1 -> buffer 0x200017CC (u16 x3).
  The single-channel variant FUN_0803ab28 converts only ch11 (PC1).
* PC1 (ch11) = battery: FUN_080782b8 / FUN_080326b4, V*100 = (raw*0x528>>12) + cal - 100 (same formula as the D890).
* PC2 (ch12) = second slow analog value, averaged in FUN_080782b8 (temperature or a detect line).
* **PC0 (ch10) = RX audio**: TIM6 update IRQ (IRQ54_Handler 0x08012380) calls FUN_08010000, which takes
  `buffer[0] >> 4` (8-bit sample) four at a time into FUN_08011000, a zero-crossing tone/AFSK decoder (stock
  APRS / signalling decode; the D890's equivalent runs on its discriminator input). TIM6 setup
  FUN_080093d4 -> FUN_0803faf8(ARR 0xF9, PSC 99) = 25,000 timer clocks per sample (4.8 kHz at a 120 MHz
  timer clock).
* Whether PC0 is flat / DC-coupled (needed for 4FSK, C4FM, GMSK) or de-emphasised audio: **unknown, scope it**.

## TX: MCU audio into the modulator -> PB9 = TIM4_CH4 PWM
* FUN_08091bd4 switches PB9 to alternate-function output (GPIOB pin mask 0x200, mode 0x1031800).
* Sample output FUN_08095c70(v) -> FUN_0801aa10(TIM4, ch 6 = CCR4, v): duty cycle = sample.
* Sample clock: TIM5 update IRQ (IRQ50_Handler 0x08012344); when state DAT == 4 it calls FUN_0800960c,
  which plays a byte table (FUN_0800960c: table, index, length) into CCR4 and stops TIM4/TIM5 at the end.
  TIM5 rate set by FUN_08091af0(ARR, PSC) e.g. `FUN_08091af0(n - 1, 9)` (FUN in fw_08090000.c:7484).
* Used during TX of signalling/data (after the baseband call setup FUN_080a0f8c / FUN_080a0fe0,
  FUN_080094a4 sets state 4): DTMF / 2-tone / 5-tone / APRS AFSK. So PB9 PWM (RC-filtered on the board)
  is injected into the TX audio path.
* PD1 (FUN_08091c2c): driven low while samples play (FUN_08009430), high again after (FUN_0800940c):
  most likely the microphone path enable/mute.
* Coupling unknown: probably AC-coupled with pre-emphasis (fine for analog and AFSK, distorts 4FSK/C4FM).
  Digital-mode TX through PB9 must be bench-tested (dummy load + service monitor / SDR) before use.

## Other analog outputs
* **PA4 = DAC1 = TX power control (APC)**: FUN_08004364(v) writes DAC1 (`(v & 0xFFF) << 4`). EXTI5 on PD5
  (baseband TX-state line, IRQ23_Handler 0x0800679c): PD5 low -> PC4 reset, PE6 reset, DAC1 = 0;
  PD5 high -> PC4 set, PE6 set, DAC1 = power level (0x20001578). (Checked in the disassembly 2026-11-22: the
  PD5-high branch writes 0x40 to GPIOE BSRR = set PE6, the low branch writes GPIOE BRR = reset PE6.)
* **PA5 = DAC2 = RX front-end tune voltage**: FUN_0800cdf8(v) writes DAC2 (DHR 0x40007414); values come
  from per-band tables indexed by frequency (fw_08090000.c ~line 80-135, tables at DAT_08090b28 / DAT_08090b18,
  interpolated between points). Fixed mid-scale = poor RX sensitivity off band centre.

## Used by the cfw (2026-11-22, cfw168)
* RX demod stays on PC0; TX audio = TMR4_CH4 PWM on PB9 at 48 kHz, CCR4 from DMA1 ch7, PD1 low while it runs
  (`drivers/dac.c`); PA4 APC + PE6/PC4 on TX (`dac_apc`); PA5 = stock tune tables from the NAND calibration block
  (`dac_tune_load_cal`, `dac_rx_tune`); PE0/PC12/PA7 = audio routing as FUN_08070a70 (`drivers/audio.c`).
* FUN_08070a70 is the audio router (PE0 = RF-chip audio, PC12 = baseband/tone audio, PA7 = speaker amp), not band
  select. The band switches are bits of RF-chip register 0x52 (idx 7 UHF RX, idx 4 VHF RX, idx 5 UHF TX).

## VOX (stock V1.07, found 2026-11-23)
* The microphone is not read by the MCU; the RF chip has the VOX detector. VOX level = settings +0x0C (RAM
  0x20009BD4; found by changing Menu > Settings > Radio Set > Other Func > VOX in the stock emulator: only that byte
  changes). `FUN_0807c0e0` writes reg 0x12 = {0x10, 0x20, 0x5C, 0x9C}[level] | 0x2000 and reg 0x13 =
  {0x18, 0x31, 0x6F, 0xAF}[level] (tables 0x080C9AD8 / 0x080C9AEC), called from the channel set-up; reg 0x52 switch 3
  is cleared while VOX is on (`FUN_08092cf4` / `FUN_0808f3b0` -> `FUN_0808f968(3, v)`).
* Detector output = **PD3**: the VOX poll at 0x08099B4C (not in the decompile; literal 0x08099BF0 = VOX-enabled flag
  0x20001055) calls `FUN_08014548(GPIOD, 8)` (IDR & mask) and, with PD3 low for 3 polls and the hold counter
  0x20001374 at 0, raises key event 0x16 (VOX TX, `FUN_08076954`); otherwise hold = VOX delay (settings +0x0D) + 5.
  PD3 = input with pull-down (stock CRL nibble 8, ODR bit 0).
* cfw: `drivers/rf.c` `vox_hw_config` (regs + switch 3, re-applied after the channel register list that resets
  0x12 / 0x13) and `vox_hw_active` (PD3 low). Emulator: `board Set "D3" false` = voice (idle level D3=1).

## Volume limit (Vol Level)
* Settings +0x3B (0 = Indoor, 1..8; found the same way). The knob (PC2 = ADC ch12) is analog; the MCU only shows
  its level (`FUN_08039d98` -> 0..15, volume bar drawn in `FUN_0804b5f8`).
* `FUN_0800e958` sends the limit to the SCT3288: attenuation table 0x0801C688 = {10 0E 0C 0A 08 06 04 02 00}[level]
  as baseband frame B0 0x40 ID 0x08 (group 2 field 0x0D of `FUN_080854dc`), and `FUN_080113c8` sends B0 0x00 ID 0x69
  = {01 51} for Indoor, {1F 40} otherwise. Stock emulator baseband.log when the menu value changes:
  `84 A9 61 00 03 00 69 01 51 00` + `84 A9 61 00 02 40 08 10` (Level 8 -> Indoor), `00 69 1F 40` + `40 08 0E`
  (Indoor -> Level 1). cfw: `drivers/audio.c` `audio_volume_limit`. Whether FM receive audio passes through that
  codec stage (PE0 router path) needs the hardware.

## GPS
* USART2 PA2 TX / PA3 RX. Stock sends `$CFGSYS,h00000001` + `$CFGMSG,0,0..9,x` (x = 1 for 0 GGA and 4 RMC) at boot and
  when GPS is switched on. No GPS power switch: Menu > GPS > On/Off in the stock emulator changes no output pin (GPIOA-E
  CRL/CRH/ODR dumps; only the pull bits of the input pins PD6 / PD11 moved once, not repeatably). cfw:
  `drivers/uart.c` `gps_uart_send_cfg`.

## I2C bus (SDA PB6, SCL PB7)
FM broadcast receiver 0x10 and LIS2DW12 accelerometer 0x19 (man down, `shared/hal/accel.c` with the stock set-up
values of `FUN_0803e148`). cfw driver `drivers/i2c.c` (push-pull, SDA switched to input pull-up to read, as stock
`FUN_0808dfdc`).
