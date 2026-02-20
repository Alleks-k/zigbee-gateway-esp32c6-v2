#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck is required but not found in PATH."
    exit 1
fi

ANALYSIS_FILES=(
    "${ROOT_DIR}/components/gateway_core/src/config_service.c"
    "${ROOT_DIR}/components/gateway_core/src/device_service.c"
    "${ROOT_DIR}/components/gateway_core/src/device_service_persistence.c"
    "${ROOT_DIR}/components/gateway_core/src/device_service_rules.c"
    "${ROOT_DIR}/components/gateway_core_state/src/gateway_state.c"
    "${ROOT_DIR}/components/gateway_core_system/src/system_service.c"
    "${ROOT_DIR}/components/gateway_core_jobs/src/job_queue.c"
    "${ROOT_DIR}/components/gateway_core_jobs/src/job_queue_state.c"
    "${ROOT_DIR}/components/gateway_core_jobs/src/job_queue_policy.c"
    "${ROOT_DIR}/components/gateway_web_api/src/api_usecases.c"
    "${ROOT_DIR}/components/gateway_web_ws/src/ws_manager.c"
    "${ROOT_DIR}/components/gateway_web_ws/src/ws_manager_state.c"
    "${ROOT_DIR}/components/gateway_web_ws/src/ws_manager_policy.c"
)

INCLUDE_DIRS=(
    "${ROOT_DIR}/tests/host/include"
    "${ROOT_DIR}/components/gateway_core/include"
    "${ROOT_DIR}/components/gateway_core_events/include"
    "${ROOT_DIR}/components/gateway_core_jobs/include"
    "${ROOT_DIR}/components/gateway_core_system/include"
    "${ROOT_DIR}/components/gateway_core_storage/include"
    "${ROOT_DIR}/components/gateway_shared_config/include"
    "${ROOT_DIR}/components/gateway_web_ws/include"
    "${ROOT_DIR}/components/gateway_web_api/include"
    "${ROOT_DIR}/components/gateway_core_facade/include"
    "${ROOT_DIR}/components/gateway_core_state/include"
    "${ROOT_DIR}/components/gateway_core_wifi/include"
    "${ROOT_DIR}/components/gateway_core_zigbee/include"
)

CPP_CHECK_ARGS=(
    --quiet
    --std=c11
    --language=c
    --enable=warning,style,performance,portability
    --error-exitcode=1
    --suppress=missingIncludeSystem
    --suppress=unusedFunction
)

for inc in "${INCLUDE_DIRS[@]}"; do
    CPP_CHECK_ARGS+=("-I${inc}")
done

cppcheck "${CPP_CHECK_ARGS[@]}" "${ANALYSIS_FILES[@]}"

echo "Static analysis check passed."
