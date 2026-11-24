# D168UV cfw codeplug ("D8CP", version 1) — board layout

The D168UV custom firmware uses **the same codeplug format as the D890UV cfw**, byte for byte: header, section
directory, all record layouts and the CRC are defined in
`D890UV/D890UV_src/cfw_cps/docs/CODEPLUG.md` (reference encoder `cfw_cps/cps/codeplug.py`). The firmware reader is the
shared `shared/app/cfwcp.c` / `cfwcp.h` (= `D890UV_src/cfw/app/`), compiled unchanged. A CPS that can write a D890UV
cfw codeplug can write this one; only the items below differ.

## What differs on the D168UV

| Item | D890UV cfw | D168UV cfw |
|---|---|---|
| Header u16 at offset 18 (D890 doc: "flags") | model id 0 = D890UV | model id **1 = D168UV** (cfw_cps `codeplug.MODELS`). Since 2026-11-22 both firmwares check it (`CFWCP_MODEL_ID`): a codeplug with another id is ignored (built-in channels, as with a bad CRC) |
| Identity reply to `02` | `'I' "D890CFW" 00 …` | `'I' "D168CFW" 00 fwMaj fwMin cpVer caps maxKB_hi maxKB_lo 06` — **this is the model/board field**: a CPS must check it before writing |
| Codeplug region (internal flash) | `0x08180000–0x081DFFFF` | `0x080A0000–0x080FFFFF` (384 KB, flash bank 2) |
| Link address space | `0x00000000–0x0005FFFF` | same: `0x00000000–0x0005FFFF` (offset from the region base) |
| Info block `0x00F00000` field "region base" | `0x08180000` | `0x080A0000` |
| Erase unit | 8 KB page | 8 KB logical page = 4 × 2 KB AT32 sectors; the same rule: erased on its first write in a session |
| Writes | 16-byte aligned, multiple of 16 | same |
| UART | USART1 PB6/PB7 115200 8N1 | USART1 **PA9 TX / PA10 RX** (programming jack, stock IRQ37), 115200 8N1. PA10 doubles as speaker-mic PTT |
| No valid codeplug | stock NAND channels + built-ins | **built-in channels only** (the stock D168 NAND channel layout is not decoded; `CFW_NAND_CODEPLUG 0`) |
| `cp_settings_t.keys[6]` | P1 P2 PF1 PF2 PF3 PF3-long | stored; the D168UV has PF1 (PA13) and PF2 (PA14) only (fixed: PF1 = next mode, PF2 = VFO/MEM) |
| Section 10 (stock CPS data) | D890UV stock tables | stored opaque; the D168 stock table set is the CPS worker's choice (cps168), the firmware only uses the accessor |
| RF | two chips | one chip (no second-receiver fields are used) |
| `make HW=1` | writes refused, caps b2 | same |

Nothing else changes: format version 1, magic `D8CP`, record sizes (settings 128, channel 64, contact 24, trunk system
48, CHEXT 32, key 56, band plan 24), section ids 1–15, CRC-32 over `[16, total)`, header block written last.

## Flash map (internal flash, AT32F403A 1 MB)

```
0x08000000-0x08003FFF  stock bootloader              never written (flash driver refuses it)
0x08004000-0x0807FFFF  cfw image (bank 1)            ~144 KB used
0x08080000-0x08081FFF  cfw settings page (8 KB)      drivers/settings.c (append-only records, CRC)
0x08082000-0x0809FFFF  unused
0x080A0000-0x080FFFFF  cfw codeplug (384 KB)         this document
```

`drivers/flash.c` only erases/programs inside the settings page and the codeplug region; everything else is refused.

## Test

Emulator round trip: `python3 emu/run.py --cfw --elf cfw/build/cfw168.elf --uart-socket usart1:3480 --serve …`
then point the CPS (or `D890UV_src/cfw_cps/tools/ws_bridge.py`-style client) at TCP 3480; see README "CPS link".
