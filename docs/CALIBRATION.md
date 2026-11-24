# RF / TX calibration: read it from the stock radio before flashing the cfw

Every radio has its own factory RF calibration: TX (PA drive per power level and band) and RX (front-end tune points, squelch thresholds and their per-frequency corrections). The
custom firmware does not calibrate anything. It reads the stock calibration block **in place** at every boot and
uses its values exactly like the stock firmware does. This page says where the block is, how to read and keep a
copy of it from the radio **while it still runs the stock firmware**, and how to check that the cfw uses the same
data after flashing.

Everything here only **reads**. Nothing writes the NAND: not the cfw, not `cal_dump.py`, not `hw.py`.

## 1. Where it lives and why the cfw needs it

| | D168UV (AT32F403A) |
|---|---|
| Storage | external SPI-NAND (not the MCU flash), two copies |
| Copy A / copy B | `0x02F60000` / `0x02F80000` (B = A + one 128 KB NAND block) |
| Valid copy | ends with `55 55 AA AA` at +`0x1FFFC` **and** its "invalid" marker at +`0x1FFF8` is not `AA AA 55 55` |
| Stock loader | `FUN_0807a64c` (stock V1.07): copy A if valid, else B (even an invalid B) |
| cfw loader | `D168UV/cfw/drivers/dac.c` `dac_tune_load_cal()`: copy A if valid, else B if valid, else **none** |
| Values the cfw uses | +0x000 UHF struct: APC cal per level at +0x02..+0x05, RX tune points u16 at +0x12/+0x14/+0x16 (400 / 440 / 480 MHz). +0x040 VHF struct: APC cal at +0x42..+0x45, tune points at +0x52/+0x54/+0x56 (136 / 155 / 174 MHz), +0x5E = 200-300 MHz APC value. APC tables one byte per MHz: UHF levels 0-3 at +0x300 + 0x80 × level (121 bytes, 400-520 MHz, reference index 40); VHF at +0x500 + 0x40 × level (39 bytes, 136-174 MHz, reference index 9). Stock reads more (up to +0x657), see `D890UV_src/tools/hw/calmap.py` |

Flashing the cfw rewrites only the MCU's internal flash. The NAND, and the calibration in it, stay as they are.
But the cfw depends on that block:
- **No valid copy = no TX.** The cfw sets the PA drive (APC DAC) to 0 and refuses PTT (pre-flash audit F5). Without
  calibration a stock-formula APC value can wrap to almost full scale.
- With a valid copy, the APC value is the stock formula, clamped to the highest value the stock tables give in that
  band (F5). The RX tune voltage is interpolated from the tune points.

So read, check and keep a copy **before** the first flash. Then you know what the radio had, and you can check that
the cfw sees the same thing.

## 1b. RX calibration and the other sub-areas

The RX calibration is in the same block as the TX calibration, so the dump in section 2 already contains it:
`cal_dump.py` saves the whole block, and the `.json` lists every sub-area the stock loader reads (`areas`: offset,
size, role, cfw use, data) and decodes the squelch calibration (`decoded.squelch`). Nothing extra to read.

What the RX values do:
- **RX front-end tune points.** Three points per band. The stock code interpolates them into a tuning voltage per
  frequency, which tunes the receiver's input filter onto the channel. Wrong or missing points cost sensitivity
  (deaf radio), especially away from the band centre. D168: both bands' tune points drive DAC2 (PA5) (stock `FUN_0809099c`); the cfw uses them (`dac_rx_tune`).
- **Squelch threshold.** Stock `FUN_0807bf6c` writes RF reg 0x49 = base (UHF +0x10, VHF +0x50) + 4 × (squelch level - 1) + correction table[reference] - table[frequency] (UHF +0x620: 25 bytes, 5 MHz steps from 400 MHz, reference index 8; VHF +0x600: 20 bytes, 2 MHz steps from 136 MHz, reference index 10), minus a temperature term (`FUN_08039694`).
  The cfw uses this calibration (2026-11-23): `drivers/dac.c dac_sql_threshold()` computes the stock threshold,
  `drivers/rf.c` writes it to reg 0x49 on every frequency change, the shared `app/radio.c` opens / closes as the stock
  squelch does (3 polls; close below threshold - 12, min 0x46, or noise > 0x50). Without a NAND block the values come
  from `drivers/cal_default.c`. Not ported: the temperature term `FUN_08039694` and the two special-mode branches that
  use a base alone. Test: `shared/tools/sql_test.py --radio d168`.
- **RSSI -> dBm.** The cfw uses a fixed conversion. No stock RSSI calibration has been identified (the squelch
  tables above are the closest thing).

| Offset | Size | Kind | What (stock users) | cfw today |
|---|---|---|---|---|
| +0x0 | 0x20 | tx+rx | UHF struct: APC cal +0x02..+0x05 (FUN_0808ca20), squelch base u16 +0x10 (FUN_0807bf6c -> reg 0x49), RX tune points +0x12/+0x14/+0x16 400/440/480 MHz (FUN_08039030); +0x1D = a byte the loader zeroes if > 0x11 | APC, tune, squelch: used |
| +0x40 | 0x20 | tx+rx | VHF struct: APC cal +0x42..+0x45, squelch base u16 +0x50, RX tune points +0x52/+0x54/+0x56 136/155/174 MHz (FUN_080390e0), +0x5E = 200-300 MHz APC value | APC, tune, squelch: used |
| +0x80 | 0xF | ? | RAM 0x2000CC68 (FUN_0800ed04, FUN_0801157c) | not used |
| +0xC0 | 0x12 | ? | RAM 0x2000D00F (FUN_0801136c, FUN_08055b70) | not used |
| +0x100 | 0x27 | ? | RAM 0x2000CC77, 39 bytes = one per MHz 136-174 (FUN_0800f188, FUN_08039908) | not used |
| +0x140 | 0x27 | ? | RAM 0x2000CC9E, as +0x100 | not used |
| +0x180 | 0x8 | ? | RAM 0x20001560 (FUN_08010e98, FUN_0803ea7c) | not used |
| +0x200 | 0x6C | ? | four 12-byte records at +0x200/+0x220/+0x240/+0x260, RAM 0x2000CCC5.. (FUN_0809127c) | not used |
| +0x300 | 0x200 | tx | UHF APC tables, 4 levels x 0x80, 121 bytes, 1 MHz steps 400-520 MHz, ref index 40 | used |
| +0x500 | 0x100 | tx | VHF APC tables, 4 levels x 0x40, 39 bytes 136-174 MHz, ref index 9 | used |
| +0x600 | 0x14 | rx | squelch frequency correction, VHF: 20 bytes, 2 MHz steps from 136 MHz, ref index 10 (156 MHz) (FUN_0807bf6c) | used |
| +0x620 | 0x19 | rx | squelch frequency correction, UHF: 25 bytes, 5 MHz steps from 400 MHz, ref index 8 (440 MHz) | used |
| +0x640 | 0x18 | ? | RAM 0x2000D054 (FUN_08097a30, FUN_08098158) | not used |

Kind: tx / rx = identified. "?" = read by the stock loader and used by the stock code named, meaning not identified
yet. The dump keeps these bytes too, so nothing is lost.

### Not used by the cfw yet (stock RX calibration)

- Squelch: used since 2026-11-23 (see above); the temperature term `FUN_08039694` is not ported.
- Unidentified sub-areas (`?` above): +0x080, +0x0C0, +0x100 / +0x140 (39 bytes each = one per MHz 136-174: a VHF per-frequency value), +0x180, +0x200..+0x26B, +0x640. These need analysis before the cfw can use them.

### Other NAND blocks near the calibration (not calibration)

- `0x02FA0000` / `0x02FC0000` (A/B, 128 KB): **runtime state the radio itself writes**. The firmware erases the
  block and writes 0x200 bytes from RAM `0x2000E317` (and defaults in `FUN_0803f3f8`). It is not factory
  calibration. `cal_dump.py` records only its markers.

### The stock firmware rewrites the calibration itself

The block is not read-only for the stock firmware:
- `FUN_0809c3a0` saves the RAM copy into the **other** copy (A <-> B ping-pong). It runs from the calibration menu
  and the PC calibration commands. It also runs **at every boot** when cal byte +0x1D differs from a settings byte
  (`FUN_08027ff8`, right after the loader). Which copy is valid can therefore change over the radio's life;
  `cal_dump.py` handles both. Bytes outside the regions the loader reads are not carried over to the new copy.
- A missing "CHECKING DATA FOR D868S!" marker at boot (3 tries, `FUN_0802e8ec`) leads to `FUN_0803cd38` ->
  `FUN_0803f614`, which erases **both** copies and writes built-in defaults. That replaces the factory values with
  generic ones.

So dump it while the radio is healthy. Never let a radio reach "factory reset" before you have a dump.

## 2. Before flashing: dump the calibration from the stock radio

1. Charged battery or bench supply, **dummy load on the antenna**. Radio on, normal mode (not the update /
   bootloader key combination). The stock CPS must be closed.
2. Connect the PC link the stock CPS uses. On both radios the stock firmware's PC link is **USB CDC**: the PC
   reply goes out on the USB endpoint (D890 `FUN_0801b7b8`, D168 `FUN_0808c63c`). On Linux it shows up as
   `/dev/ttyACM0`, on Windows as `COMx`.
3. Run, from `D890UV/D890UV_src/` (the tools are shared):
   ```bash
   python3 tools/hw/cal_dump.py --radio d168 --port /dev/ttyACM0 --tag <radio serial number>
   ```
   It sends only `PROGRAM`, `02` (ident), `R` (read) and `END`. After a read-only session the stock `END` writes
   nothing (D168 `FUN_080967cc`, read state '%'; it then restarts the radio through the watchdog, `FUN_08040248`). The radio leaves PC mode / restarts afterwards; that is normal. The one exception is a radio that was remotely stunned: it then
   re-saves its stun state, as it does after any stock CPS read. Its single send path refuses every other frame,
   `W` writes included (`check_frame`, tested). By default it reads both copies, whole blocks: 2 × 128 KB in
   16-byte requests, as the stock CPS does. Add `--quick` to read only the used part (0x800 bytes) plus the
   markers.
4. Output goes to `cal_backups/`:
   - `cal_d168_<tag>.bin`: the copy in use.
   - `cal_d168_<tag>_B.bin`: what the radio serves for copy B.
   - `cal_d168_<tag>.json`: markers, the copy in use, CRC-32, SHA-256, and the decoded values.
5. Check the printed summary:
   - `copy in use = A` (or `A or B`, see below). **Not** `NONE`.
   - `TX possible with the cfw: True`, and for each band you will use, `usable True`.
   - Tune points are plausible: not 0, not 0xFFFF, rising with frequency.
   - The squelch lines show non-blank bases (no `(blank?)`).
   - Write down the line `CRC-32 of the first 0x800 bytes: ........`.
6. Run it a second time: the CRC and SHA-256 must be the same. Copy `cal_backups/` off this PC, next to the
   `hw_backups/` of the same radio.
7. Optional cross-check over SWD, while the stock firmware still runs and after `hw.py info` / `backup`
   (`D890UV_src/docs/HARDWARE.md` section 6):
   ```bash
   python3 tools/hw/hw.py --radio d168 calram --compare cal_backups/cal_d168_<tag>.bin
   ```
   This reads the copy the stock firmware loaded into RAM, using SRAM reads only. The command refuses to run
   unless the app on the chip is stock V1.07. It must report `0 differ`. The bytes the stock loader changes after
   reading are skipped.

**How the stock `R` command serves the copies.** The handler reads the NAND at any address, with no range limit
(D890 `FUN_0812efdc`, D168 `FUN_080967cc` case 0xb). When the 128 KB block holding the address has no valid
marker, it silently reads the same offset one block higher. So reading copy A returns exactly the copy the firmware
uses, and its markers tell whether that copy is valid. The two possible results:
- `copy in use = A`: A and B read differently. A is valid and in use.
- `copy in use = A or B`: both read identically. Either A is valid and B is an identical copy, or A is invalid and
  the radio redirected to B. The bytes in use are the same either way, and so is the CRC.

## 3. After flashing: the cfw uses the same block

The cfw reads the block in place at boot (`dac_tune_load_cal()`). It needs no copy, transfer or "import".

To check it, flash a `make HW=1 HW_NOTX=1 DEBUG=1` build first, then read the RTT log:
```bash
python3 tools/hw/hw.py --radio d168 log -f
```
The boot log should contain a line
```
cal: copy A crc32 1a2b3c4d span 0x800 uhf 1 vhf 1
```
or `cal: no valid copy, TX off`. Compare it with the dump:
- The `crc32` must equal `crc32_in_use` in the `.json`.
- The copy must be A, or B when the dump said `A or B` and B is valid.

The CRC is CRC-32 (zlib / IEEE 802.3) over the first 0x800 bytes of the copy in use, the same bytes
`calmap.crc()` hashes.

**Status 2026-11-23:** the line is in the cfw (shared `hal/safety.c cal_report()`, called by `dac_tune_load_cal()`
right after choosing the copy; RTT in DEBUG=1 builds). The Info screen shows the same: `Cal A 1A2B3C4D U1 V1` or
`Cal none: TX off`. Emulator test: `shared/tools/cal_crc_test.py --radio d168` (CRC = `calmap.crc()` on
`emu/images/synth.pages`, and the no-block case). `hw.py calram` before flashing stays the cross-check.

## 4. If the block is invalid or missing

- **Do not transmit** with this radio on the cfw. The cfw already refuses TX without a valid copy; do not work
  around that.
- Do not try to "repair" the NAND, and do not write a dump back. No tool here writes the NAND, on purpose: the
  block is per-radio factory data.
- Leave the stock firmware on the radio, or restore it (`hw.py --radio d168 recover`, or the official USB updater).
  Stock behaves as before: it reads copy B anyway.
- Re-run `cal_dump.py` once. A one-off read error shows up as a different CRC between two runs.
- A radio with no valid calibration needs a factory / dealer calibration with the stock tools. This project does not
  do that.

## 5. Tests (offline and emulator)

```bash
python3 -m pytest tools/hw/tests -q
```
These are offline tests. A fake stock radio implements the redirect rule; the tests cover the write refusal and
`calram` against a fake chip.

```bash
python3 tools/hw/emu_cal_test.py --radio d168
```
This runs the **stock** firmware in Renode. A USB-CDC shim, made of CPU hooks on the stock CDC receive / send
functions, feeds the stock command parser. Copy A is the emulator's synthetic block, copy B a changed copy. Each
dump must match the expected bytes exactly in three cases: both copies valid, A invalid (served from B), and both
invalid (none).

```bash
python3 D890UV_src/cfw/tools/cal_crc_test.py --radio d168   # cfw boot report: copy + CRC-32 vs calmap.crc
python3 D890UV_src/cfw/tools/sql_test.py --radio d168       # cfw squelch: reg 0x49 = stock threshold, open / close
```
These run the **cfw** in the emulator on `emu/images/synth.pages` (no AnyTone files needed).
