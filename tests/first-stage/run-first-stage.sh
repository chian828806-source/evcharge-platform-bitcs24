#!/usr/bin/env bash
# 第一阶段六模块统一测试入口；所有构建产物写到仓库外。

set -euo pipefail

test_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${test_dir}/../.." && pwd)"
build_root="${EVCHARGE_TEST_BUILD_DIR:-$(dirname "${repo_root}")/build-first-stage-tests}"
jobs="${EVCHARGE_TEST_JOBS:-2}"

build_qmake_target() {
    local name="$1"
    local project_file="$2"
    local target_dir="${build_root}/${name}"
    mkdir -p "${target_dir}"
    (
        cd "${target_dir}"
        qmake6 "${project_file}"
        make -j"${jobs}"
    )
}

build_qmake_target server "${repo_root}/qt-server/qt-server.pro"
build_qmake_target network "${repo_root}/tests/network/network-protocol-tests.pro"
build_qmake_target admin "${repo_root}/tests/admin/admin-management-tests.pro"
build_qmake_target integration "${repo_root}/tests/integration/system-integration-tests.pro"

python3 -m unittest discover -s "${test_dir}" -p "test_*.py" -v
"${build_root}/network/network-protocol-tests"
"${build_root}/admin/tst_adminmanagement" -txt
EVCHARGE_SERVER_BINARY="${build_root}/server/evcharge-qt-server" \
    "${build_root}/integration/system-integration-tests" -txt
