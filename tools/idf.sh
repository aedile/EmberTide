#!/usr/bin/env bash
# Run idf.py inside the official espressif/idf Docker image.
# No flash/monitor support here — USB passthrough doesn't work on Docker for macOS;
# flash from the host with esptool instead (see README.md).
set -euo pipefail

IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.1}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

TTY_FLAG=""
if [ -t 0 ]; then
  TTY_FLAG="-it"
fi

exec docker run --rm ${TTY_FLAG} \
  -v "${REPO_ROOT}:/project" \
  -w /project \
  "${IDF_IMAGE}" \
  idf.py "$@"
