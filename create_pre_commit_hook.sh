#!/usr/bin/env bash
# Script to create a Git pre-commit hook on Unix-like systems
HOOK_PATH=".git/hooks/pre-commit"
cat > "$HOOK_PATH" <<'HOOK'
#!/usr/bin/env bash
pwsh -NoProfile -ExecutionPolicy Bypass -File "$(git rev-parse --show-toplevel)/pre-commit.ps1"
HOOK
chmod +x "$HOOK_PATH"
echo "pre-commit hook created at $HOOK_PATH"
