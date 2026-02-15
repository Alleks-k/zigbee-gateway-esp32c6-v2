#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-host"

mkdir -p "${BUILD_DIR}"

cc -std=c11 -Wall -Wextra -Werror \
    -I"${ROOT_DIR}/tests/host/include" \
    -I"${ROOT_DIR}/components/gateway_core/include" \
    -I"${ROOT_DIR}/components/gateway_core_storage/include" \
    -I"${ROOT_DIR}/components/gateway_shared_config/include" \
    "${ROOT_DIR}/tests/host/core_config_service_host_test.c" \
    "${ROOT_DIR}/components/gateway_core/src/config_service.c" \
    -o "${BUILD_DIR}/core_config_service_host_test"

"${BUILD_DIR}/core_config_service_host_test"

echo "All host tests passed."
