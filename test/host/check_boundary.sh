#!/usr/bin/env sh
# check_boundary.sh — CTest-visible compile-fail boundary assertion.
#
# Usage:
#   check_boundary.sh <source_file> [<include_dir> ...]
#
# Exit 0  → compilation FAILED as expected (boundary holds).
# Exit 1  → compilation SUCCEEDED unexpectedly (boundary broken).
# Exit 2  → usage / environment error.
#
# The script compiles <source_file> WITHOUT any of the include dirs that would
# provide the forbidden header. If the compilation fails (non-zero exit from
# $CC), the boundary is intact and we exit 0 (test PASSES in CTest).
# If the compilation somehow succeeds, the boundary is breached and we exit 1
# (test FAILS in CTest).

set -u

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <source_file> [<include_dir> ...]" >&2
    exit 2
fi

SOURCE_FILE="$1"
shift

if [ ! -f "${SOURCE_FILE}" ]; then
    echo "check_boundary.sh: source file not found: ${SOURCE_FILE}" >&2
    exit 2
fi

# Build an include flags string from any remaining arguments.
INCLUDE_FLAGS=""
for dir in "$@"; do
    INCLUDE_FLAGS="${INCLUDE_FLAGS} -I${dir}"
done

# Use CC from the environment, defaulting to cc.
COMPILER="${CC:-cc}"

# Compile to a temporary object file and capture the result.
TMP_OBJ="$(mktemp /tmp/boundary_check_XXXXXX.o)"

# shellcheck disable=SC2086
${COMPILER} -std=c11 -c ${INCLUDE_FLAGS} "${SOURCE_FILE}" -o "${TMP_OBJ}" \
    >/dev/null 2>&1
COMPILE_EXIT=$?

# Clean up the temp file regardless of outcome.
rm -f "${TMP_OBJ}"

if [ "${COMPILE_EXIT}" -ne 0 ]; then
    # Compile failed — boundary holds.
    echo "[PASS] boundary holds: ${SOURCE_FILE} failed to compile (as required)"
    exit 0
else
    # Compile succeeded — boundary is broken.
    echo "[FAIL] boundary BROKEN: ${SOURCE_FILE} compiled successfully — a forbidden header is reachable" >&2
    exit 1
fi
