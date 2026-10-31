# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 Dave, KY4WI
# Root entry point. Probe discovery/builds live in tools/probes/Makefile.
.DEFAULT_GOAL := all

PROBES_DIR := tools/probes
SINGLE_PROBES := $(sort $(basename $(notdir $(wildcard $(PROBES_DIR)/*.s $(PROBES_DIR)/*.S $(PROBES_DIR)/*.asm))))
DIRECTORY_PROBES := $(sort $(patsubst $(PROBES_DIR)/%/build.sh,%,$(wildcard $(PROBES_DIR)/*/build.sh)))
PROBES := $(sort $(SINGLE_PROBES) $(DIRECTORY_PROBES))

.PHONY: all probes help list clean $(PROBES)

all: probes
probes:
	@$(MAKE) -C $(PROBES_DIR) all

$(PROBES):
	@$(MAKE) -C $(PROBES_DIR) $@

list:
	@$(MAKE) -C $(PROBES_DIR) list

clean:
	@$(MAKE) -C $(PROBES_DIR) clean

help:
	@echo 'make                Build all hardware probes'
	@echo 'make list           List available probes'
	@echo 'make NAME           Build one, e.g. make panelprobe'
	@echo 'make clean          Clean intermediate files (keeps tracked .bin images)'
	@echo 'make -C tools/probes ... also works'
