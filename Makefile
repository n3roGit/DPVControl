# DPVControl Makefile
# Unified command interface for building and testing

ifeq ($(OS),Windows_NT)
       PYTHON=python
       POWERSHELL=powershell
       HOOK_CMD=$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File create_pre_commit_hook.ps1
else
       PYTHON=python3
       POWERSHELL=pwsh
       HOOK_CMD=bash ./create_pre_commit_hook.sh
endif

PIP=$(PYTHON) -m pip
PIO=$(PYTHON) -m platformio

.DEFAULT_GOAL := help

.PHONY: help install test build upload clean hook

help:
	@echo "DPVControl Makefile"
	@echo "Available targets:"
	@echo "  install  - Install Python dependencies and PlatformIO"
	@echo "  test     - Run unit tests"
	@echo "  build    - Build firmware"
	@echo "  upload   - Upload firmware to ESP32"
	@echo "  clean    - Remove build artifacts"
	@echo "  hook     - Install Git pre-commit hook"

install:
	$(PIP) install --upgrade pip
	$(PIP) install --upgrade platformio requests

test:
	$(PIO) test -e native

build:
	$(PIO) run -e esp32dev

upload:
	$(PIO) run -e esp32dev -t upload

clean:
	$(PIO) run -e esp32dev -t clean

hook:
	$(HOOK_CMD)
