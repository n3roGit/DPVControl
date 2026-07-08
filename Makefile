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

.PHONY: help install venv test build clean upload hook

help:
	@echo "DPVControl Makefile"
	@echo "Available targets:"
	@echo "  install  - Install Python dependencies, PlatformIO, and pre-commit hook"
	@echo "  venv     - Create Python virtual environment (Linux/macOS)"
	@echo "  test     - Run unit tests"
	@echo "  build    - Build firmware"
	@echo "  upload   - Upload firmware to ESP32"
	@echo "  clean    - Remove build artifacts"
	@echo "  hook     - Install git pre-commit hook"

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

install: venv hook
	$(PIP) install --upgrade pip
	$(PIP) install --upgrade platformio requests

# Install git pre-commit hook that runs tests before each commit
hook:
	@if [ -d ".git" ]; then \
		mkdir -p .git/hooks; \
		if [ "$(OS)" = "Windows_NT" ]; then \
			printf '@echo off\r\necho Running all tests before commit...\r\npython -m platformio test -e native\r\nif errorlevel 1 (\r\n    echo.\r\n    echo ERROR: Some tests failed. Commit aborted!\r\n    exit /b 1\r\n)\r\necho All tests passed. Commit allowed.\r\nexit /b 0\r\n' > .git/hooks/pre-commit; \
		else \
			printf '#!/usr/bin/env bash\nset -e\n\nSCRIPT_DIR="$$(cd "$$(dirname "$$0")/../.." && pwd)"\nif [ -f "$$SCRIPT_DIR/.venv/bin/activate" ]; then\n    . "$$SCRIPT_DIR/.venv/bin/activate"\nfi\n\necho "Running all tests before commit..."\npython3 -m platformio test -e native\necho "All tests passed. Commit allowed."\n' > .git/hooks/pre-commit; \
			chmod +x .git/hooks/pre-commit; \
		fi; \
		echo "Pre-commit hook installed."; \
	else \
		echo "Not a git repository — skipping hook installation."; \
	fi

test:
	$(PIO) test -e native

build:
	$(PIO) run -e esp32dev

upload:
	$(PIO) run -e esp32dev -t upload

clean:
	$(PIO) run -e esp32dev -t clean
