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

check_core_facade_headers_no_low_level_includes() {
    local facade_dir="${ROOT_DIR}/components/gateway_core_facade/include"
    local hits
    hits="$(
        rg -n '#include[[:space:]]+"(device_service\.h|zigbee_service\.h|wifi_service\.h|system_service\.h|job_queue\.h|state_store\.h|gateway_runtime_context\.h)"' \
            "${facade_dir}" || true
    )"
    if [[ -n "${hits}" ]]; then
        while IFS= read -r hit; do
            local rel="${hit#${ROOT_DIR}/}"
            report_violation "gateway_core_facade public header includes low-level header: ${rel}"
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

check_no_legacy_default_getters() {
    local hits
    hits="$(
        rg -n '\b(device_service_get_default|gateway_state_get_default)\s*\(' \
            "${ROOT_DIR}/components" "${ROOT_DIR}/main" "${ROOT_DIR}/tests" \
            --glob '*.[ch]' || true
    )"

    if [[ -z "${hits}" ]]; then
        return
    fi

    while IFS= read -r hit; do
        [[ -z "${hit}" ]] && continue
        report_violation "legacy default getter API is forbidden: ${hit#${ROOT_DIR}/}"
    done <<< "${hits}"
}

check_no_legacy_runtime_singleton_apis() {
    local hits
    hits="$(
        rg -n '\b(job_queue_init|job_queue_submit|job_queue_get|job_queue_get_metrics|ws_manager_init|ws_handler|ws_broadcast_status|ws_httpd_close_fn|ws_manager_get_client_count|ws_manager_set_transport_ops_for_test|ws_manager_reset_transport_ops_for_test)\s*\(' \
            "${ROOT_DIR}/components" "${ROOT_DIR}/main" "${ROOT_DIR}/tests" \
            --glob '*.[ch]' || true
    )"

    if [[ -z "${hits}" ]]; then
        return
    fi

    while IFS= read -r hit; do
        [[ -z "${hit}" ]] && continue
        report_violation "legacy singleton runtime API is forbidden: ${hit#${ROOT_DIR}/}"
    done <<< "${hits}"
}

check_web_api_dependency_rules
check_no_cmake_include_hacks
check_no_relative_source_includes
check_web_api_headers_are_facade_only
check_core_facade_headers_no_low_level_includes
check_web_api_no_low_level_idf_includes
check_no_legacy_default_getters
check_no_legacy_runtime_singleton_apis

if [[ "${violations}" -ne 0 ]]; then
    echo "Component dependency/include edge check failed."
    exit 1
fi

echo "Component dependency/include edge check passed."
