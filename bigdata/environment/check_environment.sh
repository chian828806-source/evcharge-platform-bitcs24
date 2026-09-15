#!/usr/bin/env bash
set -euo pipefail

failures=0

check_command() {
  local label="$1"
  shift
  if "$@"; then
    printf 'PASS %s\n' "$label"
  else
    printf 'FAIL %s\n' "$label" >&2
    failures=$((failures + 1))
  fi
}

printf 'EVCharge Phase 2 environment check\n'
printf 'host=%s\n' "$(hostname)"
printf 'checked_at=%s\n' "$(date '+%Y-%m-%d %H:%M:%S %z')"

check_command 'java available' java -version
check_command 'python3 available' python3 --version
check_command 'spark-submit available' spark-submit --version
check_command 'HDFS root listable' hdfs dfs -ls /
# 只创建固定的空目录验证写权限，不触碰任何 ODS/DWD 业务批次。
check_command 'HDFS /evcharge writable' bash -c 'hdfs dfs -mkdir -p /evcharge/_healthcheck && hdfs dfs -test -d /evcharge/_healthcheck'

if ((failures > 0)); then
  printf 'Environment check failed: %d check(s) failed\n' "$failures" >&2
  exit 1
fi

printf 'Environment check passed\n'
