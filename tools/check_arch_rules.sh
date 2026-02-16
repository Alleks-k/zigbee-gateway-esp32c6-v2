#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

violations=0

is_allowed_include_user() {
    local path="$1"
    case "$path" in
        components/gateway_core/src/device_service_persistence.c|\
        components/gateway_core/src/config_service.c|\
        components/gateway_core_storage/src/device_repository_nvs.c)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

is_allowed_device_repo_symbol_user() {
    local symbol="$1"
    local path="$2"

    case "$symbol" in
        device_repository_load|device_repository_save)
            case "$path" in
                components/gateway_core/src/device_service_persistence.c|\
                components/gateway_core_storage/src/device_repository_nvs.c)
                    return 0
                    ;;
            esac
            ;;
        device_repository_clear)
            case "$path" in
                components/gateway_core/src/config_service.c|\
                components/gateway_core_storage/src/device_repository_nvs.c)
                    return 0
                    ;;
            esac
            ;;
        device_repository_*)
            # Any future repository API symbol is storage-owned by default.
            case "$path" in
                components/gateway_core_storage/src/device_repository_nvs.c)
                    return 0
                    ;;
            esac
            ;;
    esac

    return 1
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
        if ! is_allowed_include_user "${path}"; then
            echo "ARCH-RULE VIOLATION (include device_repository.h outside policy files): ${rel}"
            violations=1
        fi
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

        if ! is_allowed_device_repo_symbol_user "${symbol}" "${path}"; then
            echo "ARCH-RULE VIOLATION (${symbol} used outside ownership policy): ${rel}"
            violations=1
        fi
    done <<< "${hits}"
}

check_device_repository_header_usage
check_device_repository_symbol_ownership

if [[ "${violations}" -ne 0 ]]; then
    echo "Architecture guardrails check failed."
    exit 1
fi

echo "Architecture guardrails check passed."
