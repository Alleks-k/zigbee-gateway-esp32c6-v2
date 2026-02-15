#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-selftest"
IDF_PY="${IDF_PY:-/home/alex/esp/esp-idf/tools/idf.py}"

SDKCONFIG_TMP="$(mktemp /tmp/zgw-selftest-sdkconfig.XXXXXX)"
trap 'rm -f "${SDKCONFIG_TMP}"' EXIT

"${IDF_PY}" -B "${BUILD_DIR}" \
    -D SDKCONFIG="${SDKCONFIG_TMP}" \
    -D SDKCONFIG_DEFAULTS="${ROOT_DIR}/sdkconfig.defaults;${ROOT_DIR}/tests/selftest.sdkconfig.defaults" \
    build

echo "Target self-test firmware build passed."
