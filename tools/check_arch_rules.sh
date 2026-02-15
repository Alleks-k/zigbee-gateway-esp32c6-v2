#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

violations=0

is_allowed_include_user() {
    local path="$1"
    case "$path" in
        components/gateway_core/src/device_service.c|\
        components/gateway_core/src/config_service.c|\
        components/gateway_core_storage/src/device_repository_nvs.c)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

is_allowed_load_save_user() {
    local path="$1"
    case "$path" in
        components/gateway_core/src/device_service.c|\
        components/gateway_core_storage/src/device_repository_nvs.c)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

is_allowed_clear_user() {
    local path="$1"
    case "$path" in
        components/gateway_core/src/config_service.c|\
        components/gateway_core_storage/src/device_repository_nvs.c)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

check_hits_with_policy() {
    local title="$1"
    local rg_pattern="$2"
    local policy_fn="$3"
    local hits

    hits="$(rg -n "${rg_pattern}" "${ROOT_DIR}/components" "${ROOT_DIR}/main" --glob '*.c' || true)"
    if [[ -z "${hits}" ]]; then
        return 0
    fi

    while IFS= read -r hit; do
        local rel
        rel="${hit#${ROOT_DIR}/}"
        local path
        path="${rel%%:*}"

        if ! "${policy_fn}" "${path}"; then
            echo "ARCH-RULE VIOLATION (${title}): ${rel}"
            violations=1
        fi
    done <<< "${hits}"
}

check_hits_with_policy \
    "include device_repository.h outside core policy files" \
    '#include\s+"device_repository\.h"' \
    is_allowed_include_user

check_hits_with_policy \
    "device_repository_load/save must be owned by device_service" \
    '\bdevice_repository_(load|save)\s*\(' \
    is_allowed_load_save_user

check_hits_with_policy \
    "device_repository_clear allowed only in config_service factory reset policy" \
    '\bdevice_repository_clear\s*\(' \
    is_allowed_clear_user

if [[ "${violations}" -ne 0 ]]; then
    echo "Architecture guardrails check failed."
    exit 1
fi

echo "Architecture guardrails check passed."
