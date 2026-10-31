# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Discover probes from their sources and existing, hardware-tested build recipes.
.DEFAULT_GOAL := all

SINGLE_PROBES := $(sort $(basename $(notdir $(wildcard *.s *.S *.asm))))
DIRECTORY_PROBES := $(sort $(patsubst %/build.sh,%,$(wildcard */build.sh)))
PROBES := $(sort $(SINGLE_PROBES) $(DIRECTORY_PROBES))

# A sibling NEC_78K3_Toolchain checkout works without any environment setup.
# Explicit make variables or tools on PATH are also supported.
TOOLCHAIN_BIN := $(abspath ../../../NEC_78K3_Toolchain/bin)
VBCC ?= $(if $(wildcard $(TOOLCHAIN_BIN)/vbcc78k3),$(TOOLCHAIN_BIN)/vbcc78k3,vbcc78k3)
VASM ?= $(if $(wildcard $(TOOLCHAIN_BIN)/vasm78k3_std),$(TOOLCHAIN_BIN)/vasm78k3_std,vasm78k3_std)
VLINK ?= $(if $(wildcard $(TOOLCHAIN_BIN)/vlink),$(TOOLCHAIN_BIN)/vlink,vlink)

COMMON_DEPS := $(wildcard common/*.c common/*.s common/*.h ../../include/*.inc ../../annotations/maskrom/*.inc)
.PHONY: all help list clean $(PROBES)

all: $(PROBES)
$(PROBES): %: .build/%.stamp

# Retain each existing build script as the tested compilation/link recipe:
# these probes differ in crt0, memory layout, and source dependencies.
define directory_probe
.build/$(1).stamp: $(wildcard $(1)/*.c $(1)/*.h $(1)/*.s $(1)/*.S $(1)/*.asm $(1)/*.ld $(1)/build.sh) $(COMMON_DEPS) Makefile
	@mkdir -p .build
	@VBCC="$(VBCC)" VASM="$(VASM)" VLINK="$(VLINK)" bash ./$(1)/build.sh
	@test -s $(1)/$(1).bin
	@touch $$@
	@echo "Ready: $(1)/$(1).bin"
endef
$(foreach probe,$(DIRECTORY_PROBES),$(eval $(call directory_probe,$(probe))))

define single_probe
.build/$(1).stamp: $(firstword $(wildcard $(1).s $(1).S $(1).asm)) Makefile build.sh $(COMMON_DEPS)
	@mkdir -p .build
	@VASM="$(VASM)" sh ./build.sh $(1)
	@test -s $(1).bin
	@touch $$@
	@echo "Ready: $(1).bin"
endef
$(foreach probe,$(SINGLE_PROBES),$(eval $(call single_probe,$(probe))))

help:
	@echo 'make                Build every probe'
	@echo 'make list           List automatically discovered probes'
	@echo 'make NAME           Build one probe (e.g., make panelprobe)'
	@echo 'make clean          Remove intermediate files; keep checked-in .bin images'

list:
	@printf '%s\n' $(PROBES)

# Binaries are deliberately retained in Git, so clean preserves them.
clean:
	@rm -rf .build
	@find . -type f \( -name '*.o' -o -name '*.lst' \) -delete
	@for dir in $(DIRECTORY_PROBES); do \
	  for src in "$$dir"/*.c; do \
	    [ -f "$$src" ] || continue; \
	    generated="$${src%.c}.s"; \
	    [ ! -f "$$generated" ] || rm -f "$$generated"; \
	  done; \
	done
	@rm -f phase1/buildinfo.c
	@echo 'Removed intermediate build files; checked-in .bin images preserved.'
