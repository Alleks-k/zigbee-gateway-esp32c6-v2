#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <selftest_log_file>" >&2
    exit 2
fi

LOG_FILE="$1"
if [[ ! -f "${LOG_FILE}" ]]; then
    echo "Log file not found: ${LOG_FILE}" >&2
    exit 2
fi

last_complete="$(grep -Eo 'Self-tests complete, failures=[0-9]+' "${LOG_FILE}" | tail -n 1 || true)"
last_pass="$(grep -E 'SELF_TEST_APP: Self-tests passed' "${LOG_FILE}" | tail -n 1 || true)"
last_fail="$(grep -E 'SELF_TEST_APP: Self-tests failed' "${LOG_FILE}" | tail -n 1 || true)"

if [[ -z "${last_complete}" ]]; then
    echo "FAIL: summary marker not found in log: ${LOG_FILE}" >&2
    exit 1
fi

failures="${last_complete##*=}"
if [[ -z "${failures}" ]]; then
    echo "FAIL: could not parse failures count from: ${last_complete}" >&2
    exit 1
fi

echo "Self-test summary: ${last_complete}"
if [[ -n "${last_pass}" ]]; then
    echo "Last pass marker: ${last_pass}"
fi
if [[ -n "${last_fail}" ]]; then
    echo "Last fail marker: ${last_fail}"
fi

if [[ "${failures}" != "0" ]]; then
    echo "FAIL: failures=${failures}" >&2
    exit 1
fi
if [[ -z "${last_pass}" ]]; then
    echo "FAIL: failures=0 but no 'Self-tests passed' marker found." >&2
    exit 1
fi

echo "PASS: target self-tests log is green."
