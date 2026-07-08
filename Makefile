# DPVControl Makefile
# Unified command interface for building and testing

# Detect platform
ifeq ($(OS),Windows_NT)
       PYTHON=python
       PIP=$(PYTHON) -m pip
       PIO=$(PYTHON) -m platformio
else
       # On Linux/macOS, use the venv if it exists
       VENV_DIR=.venv
       ifneq ($(wildcard $(VENV_DIR)/bin/python3),)
              PYTHON=$(VENV_DIR)/bin/python3
       else
              PYTHON=python3
       endif
       PIP=$(PYTHON) -m pip
       PIO=$(PYTHON) -m platformio
endif

.DEFAULT_GOAL := help

.PHONY: help install venv test build clean upload

help:
	@echo "DPVControl Makefile"
	@echo "Available targets:"
	@echo "  install  - Install Python dependencies and PlatformIO"
	@echo "  venv     - Create Python virtual environment (Linux/macOS)"
	@echo "  test     - Run unit tests"
	@echo "  build    - Build firmware"
	@echo "  upload   - Upload firmware to ESP32"
	@echo "  clean    - Remove build artifacts"

# Create venv (Linux/macOS only, no-op on Windows)
venv:
ifeq ($(OS),Windows_NT)
	@echo "Virtual environment not needed on Windows (use pip directly)."
else
	@if [ ! -d "$(VENV_DIR)" ]; then \
		echo "Creating virtual environment in $(VENV_DIR)..."; \
		python3 -m venv $(VENV_DIR); \
		echo "Virtual environment created. Run 'make install' next."; \
	else \
		echo "Virtual environment already exists."; \
	fi
endif

install: venv
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
