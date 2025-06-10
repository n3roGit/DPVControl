# DPVControl Makefile
# Unified command interface for building and testing

# Detect platform
ifeq ($(OS),Windows_NT)
       PYTHON=python
else
       PYTHON=python3
endif

PIP=$(PYTHON) -m pip
PIO=$(PYTHON) -m platformio

.DEFAULT_GOAL := help

.PHONY: help install test build clean upload

help:
	@echo "DPVControl Makefile"
	@echo "Available targets:"
	@echo "  install  - Install Python dependencies and PlatformIO"
	@echo "  test     - Run unit tests"
	@echo "  build    - Build firmware"
	@echo "  upload   - Upload firmware to ESP32"
	@echo "  clean    - Remove build artifacts"

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
