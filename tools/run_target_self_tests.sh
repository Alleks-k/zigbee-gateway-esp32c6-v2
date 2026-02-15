#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-selftest"

resolve_idf_py() {
    if [[ -n "${IDF_PY:-}" ]]; then
        echo "${IDF_PY}"
        return 0
    fi

    if command -v idf.py >/dev/null 2>&1; then
        command -v idf.py
        return 0
    fi

    if [[ -n "${IDF_PATH:-}" && -x "${IDF_PATH}/tools/idf.py" ]]; then
        echo "${IDF_PATH}/tools/idf.py"
        return 0
    fi

    echo "Cannot find idf.py. Set IDF_PY or source ESP-IDF environment first." >&2
    return 1
}

IDF_PY="$(resolve_idf_py)"

SDKCONFIG_TMP="$(mktemp /tmp/zgw-selftest-sdkconfig.XXXXXX)"
trap 'rm -f "${SDKCONFIG_TMP}"' EXIT

"${IDF_PY}" -B "${BUILD_DIR}" \
    -D SDKCONFIG="${SDKCONFIG_TMP}" \
    -D SDKCONFIG_DEFAULTS="${ROOT_DIR}/sdkconfig.defaults;${ROOT_DIR}/tests/selftest.sdkconfig.defaults" \
    build

echo "Target self-test firmware build passed."
