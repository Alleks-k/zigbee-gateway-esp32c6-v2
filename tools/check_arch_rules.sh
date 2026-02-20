#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

violations=0

check_core_no_repository_headers() {
    local core_dir="${ROOT_DIR}/components/gateway_core"
    local hits
    hits="$(
        rg -n '#include\s+"(device_repository\.h|config_repository\.h|storage_kv\.h|storage_schema\.h|storage_partitions\.h)"' \
            "${core_dir}/src" "${core_dir}/include" --glob '*.[ch]' || true
    )"

    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            echo "ARCH-RULE VIOLATION (gateway_core must not include repository/storage headers): ${rel}"
            violations=1
        done <<< "${hits}"
    fi
}

check_device_repository_header_usage() {
    local hits
    hits="$(rg -n '#include\s+"device_repository\.h"' "${ROOT_DIR}/components" "${ROOT_DIR}/main" --glob '*.[ch]' || true)"
    if [[ -z "${hits}" ]]; then
        return 0
    fi

    while IFS= read -r hit; do
        local rel="${hit#${ROOT_DIR}/}"
        local path="${rel%%:*}"
        case "${path}" in
            components/gateway_core_persistence_adapter/src/gateway_persistence_adapter.c|\
            components/gateway_core_storage/src/device_repository_nvs.c)
                ;;
            *)
                echo "ARCH-RULE VIOLATION (include device_repository.h outside ownership files): ${rel}"
                violations=1
                ;;
        esac
    done <<< "${hits}"
}

check_device_repository_symbol_ownership() {
    local hits
    hits="$(rg -n -o '\bdevice_repository_[a-zA-Z0-9_]+' "${ROOT_DIR}/components" "${ROOT_DIR}/main" --glob '*.c' || true)"
    if [[ -z "${hits}" ]]; then
        return 0
    fi

    while IFS= read -r hit; do
        local rel="${hit#${ROOT_DIR}/}"
        local path="${rel%%:*}"
        local symbol="${rel##*:}"
        case "${symbol}" in
            device_repository_load|device_repository_save|device_repository_clear)
                case "${path}" in
                    components/gateway_core_persistence_adapter/src/gateway_persistence_adapter.c|\
                    components/gateway_core_storage/src/device_repository_nvs.c)
                        ;;
                    *)
                        echo "ARCH-RULE VIOLATION (${symbol} used outside ownership files): ${rel}"
                        violations=1
                        ;;
                esac
                ;;
            device_repository_*)
                case "${path}" in
                    components/gateway_core_storage/src/device_repository_nvs.c)
                        ;;
                    *)
                        echo "ARCH-RULE VIOLATION (${symbol} is storage-owned by default): ${rel}"
                        violations=1
                        ;;
                esac
                ;;
        esac
    done <<< "${hits}"
}

check_core_no_repository_headers
check_device_repository_header_usage
check_device_repository_symbol_ownership

check_core_no_platform_includes() {
    local core_dir="${ROOT_DIR}/components/gateway_core"
    local hits
    hits="$(
        rg -n '#include[[:space:]]+[<"](esp_[^">]+|freertos/[^">]+)[>"]' \
            "${core_dir}/src" "${core_dir}/include" --glob '*.[ch]' || true
    )"

    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            echo "ARCH-RULE VIOLATION (gateway_core must stay platform-agnostic): ${rel}"
            violations=1
        done <<< "${hits}"
    fi
}

check_core_no_platform_includes

if [[ "${violations}" -ne 0 ]]; then
    echo "Architecture guardrails check failed."
    exit 1
fi

echo "Architecture guardrails check passed."
