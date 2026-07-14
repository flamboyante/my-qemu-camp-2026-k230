#!/usr/bin/env bash

# K230 DWC SSI qtest 彩色逐项运行器。
#
# 原始 qtest 保持 TAP 兼容。
# 本脚本只服务本地开发，逐项执行测试。
# 某个断言 abort 后，仍继续收集其余结果和失败原因。

set -u
set -o pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd "${script_dir}/../.." && pwd)
build_dir=${BUILD_DIR:-"${source_dir}/build"}
qtest_bin="${build_dir}/tests/qtest/k230-dw-ssi-test"
qemu_bin="${build_dir}/qemu-system-riscv64"
timeout_seconds=${QTEST_TIMEOUT:-15}
filter=${1:-}

if [[ ${K230_QTEST_NO_COLOR:-0} == 1 ]]; then
    color_reset=
    color_bold=
    color_green=
    color_red=
    color_yellow=
    color_cyan=
else
    color_reset=$'\033[0m'
    color_bold=$'\033[1m'
    color_green=$'\033[1;32m'
    color_red=$'\033[1;31m'
    color_yellow=$'\033[1;33m'
    color_cyan=$'\033[1;36m'
fi

print_status()
{
    local color=$1
    local label=$2
    local path=$3

    printf '%s[%s]%s %s\n' "${color}" "${label}" "${color_reset}" "${path}"
}

extract_reason()
{
    local log_file=$1
    local pattern
    local reason

    for pattern in "assertion failed" "ERROR:" "Bail out!" "not ok"; do
        reason=$(awk -v pattern="${pattern}" \
            'index($0, pattern) { print; exit }' "${log_file}")
        if [[ -n ${reason} ]]; then
            printf '%s\n' "${reason}"
            return
        fi
    done

    tail -n 1 "${log_file}"
}

print_reproduce_command()
{
    local path=$1

    printf '  %s复现:%s QTEST_QEMU_BINARY="%s" "%s" -p "%s" --tap\n' \
        "${color_bold}" "${color_reset}" "${qemu_bin}" "${qtest_bin}" "${path}"
}

printf '%s[BUILD]%s 重新编译 QEMU 和 K230 SSI qtest\n' \
    "${color_cyan}" "${color_reset}"
build_log=$(mktemp "${TMPDIR:-/tmp}/k230-ssi-build.XXXXXX")
ninja -C "${build_dir}" \
    qemu-system-riscv64 tests/qtest/k230-dw-ssi-test 2>&1 |
    tee "${build_log}"
build_status=${PIPESTATUS[0]}
if [[ ${build_status} -ne 0 ]]; then
    build_reason=$(awk '
        index($0, "error:") { print; exit }
    ' "${build_log}")
    if [[ -z ${build_reason} ]]; then
        build_reason=$(awk '
            index($0, "FAILED:") { print; exit }
        ' "${build_log}")
    fi
    if [[ -z ${build_reason} ]]; then
        build_reason="请查看上方 Ninja 日志"
    fi
    print_status "${color_red}" "BUILD FAIL" \
        "编译失败，未开始运行 qtest"
    printf '  %s原因:%s %s\n' \
        "${color_bold}" "${color_reset}" "${build_reason}"
    rm -f "${build_log}"
    exit 2
fi
rm -f "${build_log}"

mapfile -t all_tests < <(
    QTEST_QEMU_BINARY="${qemu_bin}" "${qtest_bin}" -l |
        sed -n 's/^# \(\/riscv64\/k230-dw-ssi\/.*\)$/\1/p'
)

if [[ -n ${filter} ]]; then
    if [[ ${filter} == /* ]]; then
        test_prefix=${filter}
    else
        test_prefix="/riscv64/k230-dw-ssi/${filter}"
    fi

    tests=()
    for path in "${all_tests[@]}"; do
        if [[ ${path} == "${test_prefix}"* ]]; then
            tests+=("${path}")
        fi
    done
else
    tests=("${all_tests[@]}")
fi

if [[ ${#tests[@]} -eq 0 ]]; then
    print_status "${color_red}" "NO TEST" "没有匹配 '${filter}' 的测试"
    exit 2
fi

pass=0
fail=0
timed_out=0

printf '%s[RUN]%s 共 %d 项，过滤条件：%s\n' \
    "${color_cyan}" "${color_reset}" "${#tests[@]}" "${filter:-全部}"

for path in "${tests[@]}"; do
    log_file=$(mktemp "${TMPDIR:-/tmp}/k230-ssi-qtest.XXXXXX")

    {
        timeout "${timeout_seconds}" \
            env QTEST_QEMU_BINARY="${qemu_bin}" \
            "${qtest_bin}" -p "${path}" --tap
    } >"${log_file}" 2>&1
    status=$?

    if [[ ${status} -eq 0 ]]; then
        print_status "${color_green}" "PASS" "${path}"
        pass=$((pass + 1))
    elif [[ ${status} -eq 124 ]]; then
        print_status "${color_yellow}" "TIMEOUT" "${path}"
        printf '  %s原因:%s 超过 %s 秒仍未结束\n' \
            "${color_bold}" "${color_reset}" "${timeout_seconds}"
        print_reproduce_command "${path}"
        timed_out=$((timed_out + 1))
    else
        reason=$(extract_reason "${log_file}")
        print_status "${color_red}" "FAIL" "${path}"
        printf '  %s原因:%s %s\n' \
            "${color_bold}" "${color_reset}" "${reason}"
        print_reproduce_command "${path}"
        if [[ ${QTEST_VERBOSE:-0} == 1 ]]; then
            printf '  %s完整日志:%s\n' "${color_bold}" "${color_reset}"
            sed 's/^/    /' "${log_file}"
        fi
        fail=$((fail + 1))
    fi

    rm -f "${log_file}"
done

printf '\n%s[SUMMARY]%s %sPASS=%d%s  %sFAIL=%d%s  %sTIMEOUT=%d%s  TOTAL=%d\n' \
    "${color_cyan}" "${color_reset}" \
    "${color_green}" "${pass}" "${color_reset}" \
    "${color_red}" "${fail}" "${color_reset}" \
    "${color_yellow}" "${timed_out}" "${color_reset}" \
    "${#tests[@]}"

if [[ ${fail} -ne 0 || ${timed_out} -ne 0 ]]; then
    exit 1
fi
