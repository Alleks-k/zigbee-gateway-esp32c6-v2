#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck is required but not found in PATH."
    exit 1
fi

ANALYSIS_FILES=()
while IFS= read -r -d '' file; do
    ANALYSIS_FILES+=("${file}")
done < <(find "${ROOT_DIR}/components" -type f -path '*/src/*.c' -print0 | sort -z)

if [[ "${#ANALYSIS_FILES[@]}" -eq 0 ]]; then
    echo "No source files found for static analysis."
    exit 1
fi

INCLUDE_DIRS=("${ROOT_DIR}/tests/host/include")
while IFS= read -r -d '' dir; do
    INCLUDE_DIRS+=("${dir}")
done < <(find "${ROOT_DIR}/components" -type d -name include -print0 | sort -z)

CPP_CHECK_ARGS=(
    --quiet
    --std=c11
    --language=c
    --enable=warning,performance,portability
    --error-exitcode=1
    --suppress=missingIncludeSystem
    --suppress=unusedFunction
    --suppress=knownConditionTrueFalse
)

for inc in "${INCLUDE_DIRS[@]}"; do
    CPP_CHECK_ARGS+=("-I${inc}")
done

cppcheck "${CPP_CHECK_ARGS[@]}" "${ANALYSIS_FILES[@]}"

echo "Static analysis check passed."
