#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

violations=0

report_violation() {
    local msg="$1"
    echo "COMPONENT-EDGE VIOLATION: ${msg}"
    violations=1
}

check_web_api_dependency_rules() {
    local cmake_file="${ROOT_DIR}/components/gateway_web_api/CMakeLists.txt"
    if [[ ! -f "${cmake_file}" ]]; then
        report_violation "missing file: components/gateway_web_api/CMakeLists.txt"
        return
    fi

    local forbidden_reqs=(
        gateway_core
        gateway_core_events
        gateway_core_jobs
        gateway_core_state
        gateway_core_storage
        gateway_core_system
        gateway_core_wifi
        gateway_core_zigbee
        nvs_flash
        esp_timer
    )

    for dep in "${forbidden_reqs[@]}"; do
        if rg -n "^[[:space:]]*${dep}[[:space:]]*$" "${cmake_file}" >/dev/null; then
            report_violation "gateway_web_api has forbidden direct dependency '${dep}' in CMakeLists"
        fi
    done

    if ! rg -n '^[[:space:]]*gateway_core_facade[[:space:]]*$' "${cmake_file}" >/dev/null; then
        report_violation "gateway_web_api must depend on gateway_core_facade"
    fi
}

check_no_cmake_include_hacks() {
    local hits
    hits="$(rg -n '\.\./' "${ROOT_DIR}/components" --glob '*/CMakeLists.txt' || true)"
    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            report_violation "relative include/dependency hack in ${rel}"
        done <<< "${hits}"
    fi
}

check_no_relative_source_includes() {
    local hits
    hits="$(rg -n '#include[[:space:]]+"(\.\./)+' "${ROOT_DIR}/components" --glob '*.[ch]' || true)"
    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            report_violation "relative source include found: ${rel}"
        done <<< "${hits}"
    fi
}

check_web_api_headers_are_facade_only() {
    local web_api_dir="${ROOT_DIR}/components/gateway_web_api"
    local hits
    hits="$(
        rg -n '#include[[:space:]]+"(zigbee_service\.h|wifi_service\.h|system_service\.h|job_queue\.h|device_service\.h|config_service\.h|state_store\.h|gateway_events\.h)"' \
            "${web_api_dir}/src" "${web_api_dir}/include" || true
    )"
    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            report_violation "gateway_web_api includes low-level core header: ${rel}"
        done <<< "${hits}"
    fi
}

check_web_api_no_low_level_idf_includes() {
    local web_api_dir="${ROOT_DIR}/components/gateway_web_api"
    local forbidden_patterns=(
        'nvs(_flash)?\.h'
        'esp_timer\.h'
        'freertos/[^">]+'
        'driver/[^">]+'
        'esp_wifi\.h'
        'esp_netif\.h'
        'esp_event(_base)?\.h'
        'esp_partition\.h'
        'esp_ota_ops\.h'
        'esp_spiffs\.h'
        'esp_vfs[^">]*\.h'
        'esp_private/[^">]+'
        'esp_rom/[^">]+'
        'hal/[^">]+'
        'soc/[^">]+'
        'esp_flash[^">]*\.h'
        'esp_heap_caps\.h'
        'esp_task_wdt\.h'
    )

    local hits=""
    for pattern in "${forbidden_patterns[@]}"; do
        local pattern_hits
        pattern_hits="$(
            rg -n "#include[[:space:]]+[<\"](${pattern})[>\"]" \
                "${web_api_dir}/src" "${web_api_dir}/include" || true
        )"
        if [[ -n "${pattern_hits}" ]]; then
            hits+="${pattern_hits}"$'\n'
        fi
    done

    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            [[ -z "${hit}" ]] && continue
            local rel="${hit#${ROOT_DIR}/}"
            report_violation "gateway_web_api includes forbidden low-level ESP-IDF header: ${rel}"
        done <<< "${hits}"
    fi
}

check_default_getters_bootstrap_only() {
    local hits
    hits="$(
        rg -n '\b(device_service_get_default|gateway_state_get_default)\s*\(' \
            "${ROOT_DIR}/components" "${ROOT_DIR}/main" "${ROOT_DIR}/tests" \
            --glob '*.c' || true
    )"

    if [[ -z "${hits}" ]]; then
        return
    fi

    while IFS= read -r hit; do
        [[ -z "${hit}" ]] && continue

        local abs_file="${hit%%:*}"
        local rel="${abs_file#${ROOT_DIR}/}"

        case "${rel}" in
            components/gateway_app/src/gateway_app_bootstrap.c|\
            components/gateway_core/src/device_service.c|\
            components/gateway_core_state/src/gateway_state.c|\
            main/tests/*|\
            components/*/test/*|\
            tests/host/*)
                continue
                ;;
        esac

        report_violation "default getter is forbidden outside bootstrap/tests: ${hit#${ROOT_DIR}/}"
    done <<< "${hits}"
}

check_web_api_dependency_rules
check_no_cmake_include_hacks
check_no_relative_source_includes
check_web_api_headers_are_facade_only
check_web_api_no_low_level_idf_includes
check_default_getters_bootstrap_only

if [[ "${violations}" -ne 0 ]]; then
    echo "Component dependency/include edge check failed."
    exit 1
fi

echo "Component dependency/include edge check passed."
