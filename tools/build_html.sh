#!/usr/bin/env bash
# build_html.sh — Compress HTML pages and write them as C header byte arrays.
# Run from the project root or any directory; paths are resolved relative to this script.
#
# Usage: bash tools/build_html.sh
#
# Outputs (gitignored, regenerate before building firmware):
#   manual_control.h
#   gamepad_control.h

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

python3 "$SCRIPT_DIR/gzip_to_header.py" \
  "$ROOT_DIR/html/manual_control.html" \
  "$ROOT_DIR/manual_control.h"

python3 "$SCRIPT_DIR/gzip_to_header.py" \
  "$ROOT_DIR/html/gamepad_control.html" \
  "$ROOT_DIR/gamepad_control.h"

echo "Done. Remember to re-run this script whenever HTML files change."
