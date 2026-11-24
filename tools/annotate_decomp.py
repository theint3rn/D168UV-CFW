#!/usr/bin/env python3
"""Annotate the D168UV stock decompile: replace DAT_xxxxxxxx literal-pool words that hold peripheral
addresses with readable names (GPIOA, GPIOB+0x10 ...). Usage: annotate_decomp.py OUTDIR"""
import glob, os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BIN = open(os.path.join(ROOT, "fw", "d168uv_v107_08004000.bin"), "rb").read()
BASE = 0x08004000
PER = {0x40010800: "GPIOA", 0x40010C00: "GPIOB", 0x40011000: "GPIOC", 0x40011400: "GPIOD", 0x40011800: "GPIOE",
       0x40010000: "AFIO", 0x40010400: "EXTI", 0x40021000: "CRM", 0x40012400: "ADC1", 0x40012800: "ADC2",
       0x40007400: "DAC", 0x40003800: "SPI2", 0x40013000: "SPI1", 0x40003C00: "SPI3", 0x40013800: "USART1",
       0x40004400: "USART2", 0x40004800: "USART3", 0x40004C00: "UART4", 0x40005000: "UART5",
       0x40000000: "TMR2", 0x40000400: "TMR3", 0x40000800: "TMR4", 0x40000C00: "TMR5", 0x40001000: "TMR6",
       0x40001400: "TMR7", 0x40012C00: "TMR1", 0x40013400: "TMR8", 0x40020000: "DMA1", 0x40020400: "DMA2",
       0x40022000: "FLASH", 0x40007000: "PWC", 0x40002800: "RTC", 0x40006C00: "BPR", 0x40005400: "I2C1",
       0x40005800: "I2C2", 0x40014C00: "TMR9", 0x40015000: "TMR10", 0x40015400: "TMR11", 0x40018000: "SDIO",
       0xE000E010: "SYSTICK", 0xE000E100: "NVIC", 0xE000ED00: "SCB", 0x40005C00: "USBD", 0x40016800: "ACC",
       0xA0000000: "XMC_REG", 0x40002C00: "WWDT", 0x40003000: "WDT"}
def name(v):
    best = None
    for b, n in PER.items():
        if b <= v < b + 0x400 and (best is None or b > best[0]):
            best = (b, n)
    if best:
        return best[1] + ("" if v == best[0] else "+0x%X" % (v - best[0]))
    return None
def word(a):
    o = a - BASE
    return struct.unpack_from("<I", BIN, o)[0] if 0 <= o <= len(BIN) - 4 else None
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
pat = re.compile(r"\b(PTR_)?DAT_(0[0-9a-f]{7})\b")
for f in sorted(glob.glob(os.path.join(ROOT, "decomp", "src", "*.c"))):
    def rep(m):
        v = word(int(m.group(2), 16)); n = name(v) if v is not None else None
        return "%s/*%s*/" % (m.group(0), n) if n else m.group(0)
    open(os.path.join(out, os.path.basename(f)), "w").write(pat.sub(rep, open(f).read()))
