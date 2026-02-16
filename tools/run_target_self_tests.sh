#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-selftest"

resolve_idf_root() {
    if [[ -n "${IDF_PATH:-}" ]]; then
        local root
        root="$(cd "${IDF_PATH}" && pwd -P)"
        if [[ ! -x "${root}/tools/idf.py" ]]; then
            echo "IDF_PATH is set to '${IDF_PATH}', but '${root}/tools/idf.py' is missing." >&2
            return 1
        fi
        if [[ -n "${IDF_PY:-}" ]]; then
            local idf_py_real
            idf_py_real="$(cd "$(dirname "${IDF_PY}")" && pwd -P)/$(basename "${IDF_PY}")"
            if [[ "${idf_py_real}" != "${root}/tools/idf.py" ]]; then
                echo "IDF_PATH and IDF_PY point to different ESP-IDF installations." >&2
                echo "  IDF_PATH -> ${root}" >&2
                echo "  IDF_PY   -> ${idf_py_real}" >&2
                return 1
            fi
        fi
        echo "${root}"
        return 0
    fi

    local idf_py_candidate=""
    if [[ -n "${IDF_PY:-}" ]]; then
        idf_py_candidate="${IDF_PY}"
    elif command -v idf.py >/dev/null 2>&1; then
        idf_py_candidate="$(command -v idf.py)"
    fi

    if [[ -n "${idf_py_candidate}" ]]; then
        local idf_py_real
        idf_py_real="$(cd "$(dirname "${idf_py_candidate}")" && pwd -P)/$(basename "${idf_py_candidate}")"
        case "${idf_py_real}" in
            */tools/idf.py)
                echo "${idf_py_real%/tools/idf.py}"
                return 0
                ;;
            *)
                echo "Cannot derive a single IDF_PATH from '${idf_py_real}'." >&2
                echo "Set IDF_PATH explicitly (and keep IDF_PY unset)." >&2
                return 1
                ;;
        esac
    fi

    echo "Cannot find ESP-IDF. Set IDF_PATH or source ESP-IDF environment first." >&2
    return 1
}

IDF_PATH="$(resolve_idf_root)"
export IDF_PATH
IDF_PY="${IDF_PATH}/tools/idf.py"

SDKCONFIG_TMP="$(mktemp /tmp/zgw-selftest-sdkconfig.XXXXXX)"
trap 'rm -f "${SDKCONFIG_TMP}"' EXIT

rm -rf "${BUILD_DIR}"

"${IDF_PY}" -B "${BUILD_DIR}" \
    -D SDKCONFIG="${SDKCONFIG_TMP}" \
    -D SDKCONFIG_DEFAULTS="${ROOT_DIR}/sdkconfig.defaults;${ROOT_DIR}/sdkconfig.selftest" \
    reconfigure build

echo "Target self-test firmware build passed."
