#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-host"

mkdir -p "${BUILD_DIR}"

cc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/tests/host/include" \
    -I"${ROOT_DIR}/components/gateway_core/include" \
    -I"${ROOT_DIR}/components/gateway_core_persistence_adapter/include" \
    -I"${ROOT_DIR}/components/gateway_core_storage/include" \
    -I"${ROOT_DIR}/components/gateway_shared_config/include" \
    "${ROOT_DIR}/tests/host/core_config_service_host_test.c" \
    "${ROOT_DIR}/components/gateway_core_persistence_adapter/src/gateway_persistence_adapter.c" \
    "${ROOT_DIR}/components/gateway_core/src/config_service.c" \
    -o "${BUILD_DIR}/core_config_service_host_test"

"${BUILD_DIR}/core_config_service_host_test"

cc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/tests/host/include" \
    -I"${ROOT_DIR}/components/gateway_core/include" \
    -I"${ROOT_DIR}/components/gateway_core_storage/include" \
    -I"${ROOT_DIR}/components/gateway_shared_config/include" \
    "${ROOT_DIR}/tests/host/device_service_rules_host_test.c" \
    "${ROOT_DIR}/components/gateway_core/src/device_service_rules.c" \
    -o "${BUILD_DIR}/device_service_rules_host_test"

"${BUILD_DIR}/device_service_rules_host_test"

cc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/tests/host/include" \
    -I"${ROOT_DIR}/components/gateway_core/include" \
    -I"${ROOT_DIR}/components/gateway_core_persistence_adapter/include" \
    -I"${ROOT_DIR}/components/gateway_core_storage/include" \
    -I"${ROOT_DIR}/components/gateway_shared_config/include" \
    "${ROOT_DIR}/tests/host/factory_reset_policy_host_test.c" \
    "${ROOT_DIR}/components/gateway_core_persistence_adapter/src/gateway_persistence_adapter.c" \
    "${ROOT_DIR}/components/gateway_core/src/config_service.c" \
    -o "${BUILD_DIR}/factory_reset_policy_host_test"

"${BUILD_DIR}/factory_reset_policy_host_test"

cc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/tests/host/include" \
    -I"${ROOT_DIR}/components/gateway_core/include" \
    -I"${ROOT_DIR}/components/gateway_web_api/include" \
    -I"${ROOT_DIR}/components/gateway_core_facade/include" \
    -I"${ROOT_DIR}/components/gateway_core_state/include" \
    -I"${ROOT_DIR}/components/gateway_core_wifi/include" \
    -I"${ROOT_DIR}/components/gateway_core_zigbee/include" \
    -I"${ROOT_DIR}/components/gateway_shared_config/include" \
    "${ROOT_DIR}/tests/host/api_usecases_host_test.c" \
    "${ROOT_DIR}/components/gateway_web_api/src/api_usecases.c" \
    -o "${BUILD_DIR}/api_usecases_host_test"

"${BUILD_DIR}/api_usecases_host_test"

echo "All host tests passed."
