# =========================================================================
# Projet 32X (kit32x) : make → <projet>.32x
#
#   src/*.c, src/*.S        le jeu, pour les SH2 (les .S sont optionnels)
#   assets/assets.txt       la liste des images (voir kit32x/img2c.c)
#   kit32x/                 démarrage, carte mémoire, accès au matériel
#   icd_mars.bin            l'ICD 32X de Sega (1040 octets), non fourni
#
# Cibles : make, make clean, make test (BlastEm headless, voir
# kit32x/test/run_rom.sh).
# Options : make AUTOPAD=0x48 (manette simulée, octet SACBRLDU : la ROM
#           s'appelle alors <projet>-autopad.32x), TITLE="MON JEU",
#           ROM_SIZE=524288.
#
# GNU make 3.81 suffit (celui de macOS).
# =========================================================================

PROJECT  ?= $(notdir $(CURDIR))
TITLE    ?= $(PROJECT)
# 256 Ko par défaut
ROM_SIZE ?= 262144
ICD      ?= icd_mars.bin

KIT   := kit32x
BUILD := build

# ---- Outils ----------------------------------------------------------------
# Le compilateur construit par kit32x/build-cross.sh est dans ~/opt/cross.
export PATH := $(HOME)/opt/cross/bin:$(PATH)

SH     ?= sh-elf-
M68K   ?= $(if $(shell command -v m68k-elf-as 2>/dev/null),m68k-elf-,m68k-linux-gnu-)
HOSTCC ?= cc

# ---- Options de compilation SH2 --------------------------------------------
# -fno-tree-loop-distribute-patterns : gcc ne doit pas remplacer une boucle
# par memcpy/memset (le frame buffer et la palette exigent des accès 16 bits).
CFLAGS  := -m2 -mb -O2 -std=c99 -ffreestanding -fno-builtin \
           -fno-tree-loop-distribute-patterns -Wall -Wextra \
           -I$(KIT) -Isrc -I$(BUILD) $(EXTRA_CFLAGS)
ASFLAGS := -m2 -mb -Wa,--isa=sh2 -I$(KIT) -Isrc -I$(BUILD)
LDFLAGS := -m2 -mb -nostdlib -T $(KIT)/sh2.ld -Wl,-Map,$(BUILD)/sh2.map \
           -Wl,--no-warn-rwx-segments

# ---- Fichiers ---------------------------------------------------------------
ifneq ($(wildcard assets/assets.txt),)
ASSETS_C := $(BUILD)/assets.c
ASSETS_H := $(BUILD)/assets.h
endif

# sh2_start.o en premier : les vecteurs en 0600 0000h
OBJS := $(BUILD)/kit/sh2_start.o $(BUILD)/kit/hw32x.o \
        $(patsubst src/%.c,$(BUILD)/src/%.o,$(wildcard src/*.c)) \
        $(patsubst src/%.S,$(BUILD)/src/%.o,$(wildcard src/*.S)) \
        $(ASSETS_C:.c=.o)

ifdef AUTOPAD
ROM      := $(PROJECT)-autopad.32x
PAD_DEF  := --defsym AUTOPAD=$(AUTOPAD)
else
ROM      := $(PROJECT).32x
endif

.PHONY: all clean test
all: $(ROM)

# ---- Images -----------------------------------------------------------------
$(BUILD)/img2c: $(KIT)/img2c.c $(KIT)/assets_types.h
	@mkdir -p $(@D)
	$(HOSTCC) -std=c99 -O2 -I$(KIT) -o $@ $<

$(BUILD)/assets.c: assets/assets.txt $(wildcard assets/*.bmp) $(BUILD)/img2c
	$(BUILD)/img2c assets/assets.txt $(BUILD)/assets
$(BUILD)/assets.h: $(BUILD)/assets.c

# ---- SH2 --------------------------------------------------------------------
$(BUILD)/kit/%.o: $(KIT)/%.c | $(ASSETS_H)
	@mkdir -p $(@D)
	$(SH)gcc $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/kit/%.o: $(KIT)/%.S
	@mkdir -p $(@D)
	$(SH)gcc $(ASFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/src/%.o: src/%.c | $(ASSETS_H)
	@mkdir -p $(@D)
	$(SH)gcc $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/src/%.o: src/%.S | $(ASSETS_H)
	@mkdir -p $(@D)
	$(SH)gcc $(ASFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/assets.o: $(BUILD)/assets.c
	$(SH)gcc $(CFLAGS) -c $< -o $@

$(BUILD)/sh2.bin: $(OBJS) $(KIT)/sh2.ld
	$(SH)gcc $(LDFLAGS) -o $(BUILD)/sh2.elf $(OBJS) -lgcc
	$(SH)objcopy -O binary -j .vectors -j .text -j .rodata -j .data \
	  $(BUILD)/sh2.elf $@

# ---- 68000 et ROM -----------------------------------------------------------
$(BUILD)/title.inc: Makefile
	@mkdir -p $(@D)
	@printf '        .ascii "%-48.48s"\n        .ascii "%-48.48s"\n' \
	  "$(TITLE)" "$(TITLE)" > $@

# L'image SH2 est copiée par mots longs : taille arrondie à 4 octets.
$(ROM): $(BUILD)/sh2.bin $(KIT)/md68k.s $(BUILD)/title.inc $(ICD)
	@test "$$(wc -c < $(ICD))" -eq 1040 || \
	  { echo "$(ICD) doit faire 1040 octets (ICD 32X de Sega)" >&2; exit 1; }
	@size=$$(( ($$(wc -c < $(BUILD)/sh2.bin) + 3) / 4 * 4 )); \
	if [ $$((65536 + size)) -gt $(ROM_SIZE) ]; then \
	  echo "Image SH2 trop grande ($$size octets) pour ROM_SIZE" >&2; exit 1; fi; \
	set -x; \
	$(M68K)as -m68000 -I $(dir $(ICD)) -I $(BUILD) $(PAD_DEF) \
	  --defsym SH2_SIZE=$$size --defsym ROM_SIZE=$(ROM_SIZE) \
	  -o $(BUILD)/md68k.o $(KIT)/md68k.s && \
	$(M68K)ld -Ttext=0 -e 0 -o $(BUILD)/md68k.elf $(BUILD)/md68k.o && \
	$(M68K)objcopy -O binary -j .text $(BUILD)/md68k.elf $(BUILD)/md68k.bin && \
	dd if=/dev/zero bs=1024 count=$$(( $(ROM_SIZE) / 1024 )) 2>/dev/null | \
	  cat $(BUILD)/md68k.bin $(BUILD)/sh2.bin - | head -c $(ROM_SIZE) > $@
	@echo "$@ : image SH2 de $$(wc -c < $(BUILD)/sh2.bin | tr -d ' ') octets en 10000h"

# ---- Divers -----------------------------------------------------------------
test: $(ROM)
	$(KIT)/test/run_rom.sh $(ROM) 300 $(BUILD)/test

clean:
	rm -rf $(BUILD) $(PROJECT).32x $(PROJECT)-autopad.32x

-include $(OBJS:.o=.d)
