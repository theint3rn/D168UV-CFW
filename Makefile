# D168UV custom firmware (Artery AT32F403A, runs under the stock bootloader at 0x08004000)
#   make              -> build/cfw168.elf, build/cfw168.bin (load at 0x08004000), build/cfw168.map
#   make EMU=1        -> emulator test build in build_emu/ (symbol-FIFO modem backend). Never flash this build.
#   make HW=1         -> build for a real radio (build_hw/): settings / codeplug never written, digital TX compiled out,
#                        TX at Low until a power level is picked, 180 s TX hard cap, fault handlers reset with TX off
#   make HW=1 HW_DTX=1 -> ... with digital TX (TMR4_CH4 PWM on PB9 = stock MCU TX audio pin, unverified, DUMMY LOAD ONLY)
#   make HW=1 HW_NOTX=1 -> ... every TX refused ("TX inhibited"): first smoke test on a real radio
#   make HW=1 HW_SAVE=1 -> ... CPS link (USART1 PA9/PA10) + codeplug/settings flash writes (range-checked).
#                        ONLY AFTER THE OWNER OKs FLASH WRITES. Jack PTT (PA10 = link RX) not read in this build.
#   make HW=1 HW_SAVE=1 HW_PASSWORD=1 -> ... enforces the stock power-on password (other HW builds ignore it, so nobody is locked out)
#   make KEEP_SWD=1   -> PA13/PA14 stay SWD (side keys PF1/PF2 then do nothing)
#   make DEBUG=1      -> debug build in <build>_dbg/: KEEP_SWD=1 + SEGGER RTT log (hal/dbg.h, read with tools/hw/hw.py log)
#   make gdb          -> gdb-multiarch on the ELF via tools/hw/gdb/<radio>.gdb (needs a running OpenOCD or JLinkGDBServer)
#   make emu          -> build + 6 s in the D168 Renode emulator (../emu/run.py), screen in ../emu/out/cfw168/
#
# Everything above the board layer is SHARED with the D890UV cfw (shared -> ../../D890UV/D890UV_src/cfw):
# protocol libraries (proto/*), MCU-DSP (dsp/), vocoders (voc/), modem / voice / UI / codeplug / CPS link (hal/, app/,
# gfx/). Board differences come in through include/board_cfg.h (picked up by shared hal/hal.h and app/cfwcp.h).
# This tree only holds the AT32F403A drivers, startup, linker script and main.c.

CROSS   ?= arm-none-eabi-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size
SH      := shared

BUILD   := build
ifeq ($(EMU),1)
BUILD   := build_emu
endif
ifeq ($(HW),1)
BUILD   := build_hw
endif
# one directory per variant: objects built with different flags must not mix (dependencies do not track CFLAGS)
BUILD   := $(BUILD)$(if $(filter 1,$(HW_DTX)),_dtx)$(if $(filter 1,$(HW_NOTX)),_notx)$(if $(filter 1,$(HW_SAVE)),_save)$(if $(filter 1,$(HW_PASSWORD)),_pw)
ifeq ($(DEBUG),1)
BUILD   := $(BUILD)_dbg
KEEP_SWD := 1
endif
# BUILD_SUFFIX=_x: own build dir (build_x/...) when several people build at once
BUILD   := $(BUILD)$(BUILD_SUFFIX)
TARGET  := $(BUILD)/cfw168

SRCS := startup/startup.c $(wildcard drivers/*.c) app/main.c \
        $(wildcard $(SH)/hal/*.c) \
        $(wildcard $(SH)/gfx/*.c) \
        $(filter-out $(SH)/app/main.c,$(wildcard $(SH)/app/*.c)) \
        $(wildcard $(SH)/app/protos/*.c) \
        $(wildcard $(SH)/dsp/*.c) \
        $(wildcard $(SH)/voc/*.c) $(SH)/voc/imbe/voc_imbe.c $(SH)/voc/imbe/imbe_dec.c $(SH)/voc/imbe/imbe_enc.c \
        $(SH)/voc/imbe/imbe_tables.c
PROTO_SRCS := $(wildcard $(SH)/proto/dmr_nxdn/common/*.c $(SH)/proto/dmr_nxdn/dmr/*.c $(SH)/proto/dmr_nxdn/nxdn/*.c) \
              $(wildcard $(SH)/proto/p25/src/*.c) \
              $(wildcard $(SH)/proto/dstar_ysf/common/*.c $(SH)/proto/dstar_ysf/dstar/*.c $(SH)/proto/dstar_ysf/ysf/*.c)
PROTO_INC  := -I$(SH)/proto/dmr_nxdn/common -I$(SH)/proto/dmr_nxdn/dmr -I$(SH)/proto/dmr_nxdn/nxdn \
              -I$(SH)/proto/p25/include \
              -I$(SH)/proto/dstar_ysf/common -I$(SH)/proto/dstar_ysf/dstar -I$(SH)/proto/dstar_ysf/ysf
SRCS += $(PROTO_SRCS)

INC  := -Iinclude -Ihal -Idrivers -I$(SH)/hal -I$(SH)/gfx -I$(SH)/app -I$(SH)/dsp -I$(SH)/voc -I$(SH)/voc/imbe $(PROTO_INC)

CPU    := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS := $(CPU) -std=gnu11 -Os -g3 -Wall -Wextra -Wno-override-init -Wno-unused-parameter \
          -ffunction-sections -fdata-sections -fno-common -ffreestanding $(INC)
LDFLAGS := $(CPU) -T ld/at32f403a_cfw.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=$(TARGET).map \
           -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs

ifeq ($(EMU),1)
CFLAGS += -DCFW_EMU
endif
ifeq ($(HW),1)
CFLAGS += -DCFW_HW_BUILD
ifeq ($(HW_SAVE),1)
# HW_SAVE=1 (pre-flash audit F10): CPS link + codeplug / settings flash writes on a real radio, still range-checked.
# ONLY AFTER THE OWNER OKs FLASH WRITES on that radio.
CFLAGS += -DCFW_PROG_HW
# D168UV: link on USART1 PA9 / PA10 (programming jack); the jack PTT on PA10 is not read in this build
else
CFLAGS += -DCFW_HW_NO_SAVE
endif
# HW_PASSWORD=1: enforce the stock power-on password on a real radio. Only with HW_SAVE=1 (the CPS link is the way back)
ifeq ($(HW_PASSWORD),1)
ifneq ($(HW_SAVE),1)
$(error HW_PASSWORD=1 needs HW_SAVE=1: without the CPS link a forgotten password could only be cleared with SWD)
endif
CFLAGS += -DCFW_HW_PASSWORD
endif
ifeq ($(HW_DTX),1)
CFLAGS += -DCFW_DIGITAL_TX
endif
else
CFLAGS += -DCFW_DIGITAL_TX
endif
# HW_NOTX=1 (F7): every TX refused ("TX inhibited"), for the first smoke test on a real radio
ifeq ($(HW_NOTX),1)
CFLAGS += -DCFW_HW_NOTX
endif
# F8: version / build string (boot screen, RTT boot line, CPS link info block)
CFW_VER  := 0.6
CFW_KIND := $(if $(filter 1,$(HW)),HW,$(if $(filter 1,$(EMU)),EMU,SIM))$(if $(filter 1,$(HW_DTX)),+DTX)$(if $(filter 1,$(HW_NOTX)),+NOTX)$(if $(filter 1,$(HW_SAVE)),+SAVE)$(if $(filter 1,$(HW_PASSWORD)),+PW)$(if $(filter 1,$(DEBUG)),+DBG)
CFW_DATE := $(shell date +%y%m%d)
CFLAGS += -DVERSION='"$(CFW_VER)"' -DCFW_BUILD='"D168 $(CFW_KIND) $(CFW_DATE)"'

ifeq ($(KEEP_SWD),1)
CFLAGS += -DCFW_KEEP_SWD
endif
ifeq ($(DEBUG),1)
CFLAGS += -DCFW_RTT -DCFW_DEBUG
endif

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
FPU_OPT := -fbuiltin -fno-math-errno -fsingle-precision-constant
$(BUILD)/$(SH)/dsp/%.o: CFLAGS += $(FPU_OPT)
$(BUILD)/$(SH)/voc/%.o: CFLAGS += $(FPU_OPT)
$(BUILD)/$(SH)/hal/dsp_backend.o: CFLAGS += $(FPU_OPT)
$(BUILD)/$(SH)/hal/voice.o: CFLAGS += $(FPU_OPT)
DEPS := $(OBJS:.o=.d)

all: $(TARGET).elf $(TARGET).bin

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJS) ld/at32f403a_cfw.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -lm -lgcc
	$(SIZE) $@
	@$(CROSS)nm $@ | grep -q " FUN_08013d30$$" || (echo "missing delay_us alias FUN_08013d30" && false)
	@$(CROSS)nm $@ | grep -q " FUN_0807bca8$$" || (echo "missing RF read alias FUN_0807bca8" && false)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

emu: all
	python3 ../emu/run.py --elf $(TARGET).elf --seconds 6 --name cfw168 --systick 15000000

GDB ?= gdb-multiarch
GDBPORT ?= 3333
HWTOOLS := $(firstword $(wildcard tools/hw) ../../D890UV/D890UV_src/tools/hw)
gdb: $(TARGET).elf
	$(GDB) -x $(HWTOOLS)/gdb/d168.gdb -ex 'target extended-remote :$(GDBPORT)' $(TARGET).elf

clean:
	rm -rf build build_emu build_hw build_dbg build_emu_dbg build_hw_dbg

# host tests of the shared libraries + the NXDN glue loopback (shared = D890UV cfw tree; no target build needed)
test-libs:
	$(MAKE) -C $(SH) test-libs

-include $(DEPS)
.PHONY: all clean emu gdb test-libs
