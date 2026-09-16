#!/usr/bin/env bash
set -euo pipefail

required_commands=(hdfs yarn jps)
for command_name in "${required_commands[@]}"; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Missing command: ${command_name}. Open a new login shell or load the big-data environment first." >&2
    exit 1
  fi
done

is_running() {
  local process_name="$1"
  jps | awk '{print $2}' | grep -qx "${process_name}"
}

start_component() {
  local process_name="$1"
  shift
  if is_running "${process_name}"; then
    printf 'RUNNING %s\n' "${process_name}"
  else
    "$@"
    printf 'STARTED %s\n' "${process_name}"
  fi
}

stop_component() {
  local process_name="$1"
  shift
  if is_running "${process_name}"; then
    "$@"
    printf 'STOPPED %s\n' "${process_name}"
  else
    printf 'NOT_RUNNING %s\n' "${process_name}"
  fi
}

show_status() {
  local failures=0
  for process_name in NameNode DataNode SecondaryNameNode ResourceManager NodeManager; do
    if is_running "${process_name}"; then
      printf 'PASS %s\n' "${process_name}"
    else
      printf 'FAIL %s\n' "${process_name}" >&2
      failures=$((failures + 1))
    fi
  done
  hdfs dfsadmin -report | sed -n '/Live datanodes/,+3p'
  yarn node -list
  return "${failures}"
}

case "${1:-status}" in
  start)
    start_component NameNode hdfs --daemon start namenode
    start_component DataNode hdfs --daemon start datanode
    start_component SecondaryNameNode hdfs --daemon start secondarynamenode
    start_component ResourceManager yarn --daemon start resourcemanager
    start_component NodeManager yarn --daemon start nodemanager
    sleep 5
    hdfs dfs -mkdir -p \
      /user/"$(whoami)" \
      /evcharge/raw \
      /evcharge/ods \
      /evcharge/quality \
      /evcharge/rejects \
      /evcharge/dwd
    show_status
    ;;
  stop)
    stop_component NodeManager yarn --daemon stop nodemanager
    stop_component ResourceManager yarn --daemon stop resourcemanager
    stop_component SecondaryNameNode hdfs --daemon stop secondarynamenode
    stop_component DataNode hdfs --daemon stop datanode
    stop_component NameNode hdfs --daemon stop namenode
    ;;
  status)
    show_status
    ;;
  *)
    echo "Usage: $0 {start|stop|status}" >&2
    exit 2
    ;;
esac
