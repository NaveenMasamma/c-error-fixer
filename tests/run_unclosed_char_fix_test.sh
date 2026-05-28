#!/usr/bin/env bash
set -euo pipefail

prog="$1"
src_path="$(cd "$(dirname "$0")" && pwd)/test_unclosed_char.c"

# send 'n' to decline applying fixes when prompted
printf 'n\n' | "$prog" "$src_path" 2>&1 | grep -q "=== Fix Suggestions ==="
