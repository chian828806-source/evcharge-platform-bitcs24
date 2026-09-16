#!/usr/bin/env bash
# Manage the single-node Hadoop services required by the EVCharge data pipeline.

set -euo pipefail

action="${1:-status}"
profile="${2:-all}"

if [[ "${profile}" != "all" && "${profile}" != "hdfs" ]]; then
  echo "Profile must be 'all' or 'hdfs'." >&2
  exit 2
fi

required_commands=(hdfs jps)
if [[ "${profile}" == "all" ]]; then
  required_commands+=(yarn)
fi
for command_name in "${required_commands[@]}"; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Missing command: ${command_name}. Open a login shell or load the big-data environment first." >&2
    exit 1
  fi
done

hdfs_processes=(NameNode DataNode SecondaryNameNode)
yarn_processes=(ResourceManager NodeManager)

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

wait_for_component() {
  local process_name="$1"
  local attempt
  for attempt in {1..15}; do
    if is_running "${process_name}"; then
      return 0
    fi
    sleep 1
  done
  echo "Timed out waiting for ${process_name}." >&2
  return 1
}

show_status() {
  local failures=0
  local process_name
  local processes=("${hdfs_processes[@]}")
  if [[ "${profile}" == "all" ]]; then
    processes+=("${yarn_processes[@]}")
  fi
  for process_name in "${processes[@]}"; do
    if is_running "${process_name}"; then
      printf 'PASS %s\n' "${process_name}"
    else
      printf 'FAIL %s\n' "${process_name}" >&2
      failures=$((failures + 1))
    fi
  done
  hdfs dfsadmin -report | sed -n '/Live datanodes/,+3p'
  if [[ "${profile}" == "all" ]]; then
    yarn node -list
  fi
  return "${failures}"
}

create_project_directories() {
  hdfs dfs -mkdir -p \
    /user/"$(whoami)" \
    /evcharge/raw \
    /evcharge/ods \
    /evcharge/quality \
    /evcharge/rejects \
    /evcharge/dwd \
    /evcharge/dws \
    /evcharge/ads \
    /evcharge/ml
}

case "${action}" in
  start)
    start_component NameNode hdfs --daemon start namenode
    start_component DataNode hdfs --daemon start datanode
    start_component SecondaryNameNode hdfs --daemon start secondarynamenode
    wait_for_component NameNode
    wait_for_component DataNode
    wait_for_component SecondaryNameNode
    if [[ "${profile}" == "all" ]]; then
      start_component ResourceManager yarn --daemon start resourcemanager
      start_component NodeManager yarn --daemon start nodemanager
      wait_for_component ResourceManager
      wait_for_component NodeManager
    fi
    create_project_directories
    show_status
    ;;
  stop)
    if [[ "${profile}" == "all" ]]; then
      stop_component NodeManager yarn --daemon stop nodemanager
      stop_component ResourceManager yarn --daemon stop resourcemanager
    fi
    stop_component SecondaryNameNode hdfs --daemon stop secondarynamenode
    stop_component DataNode hdfs --daemon stop datanode
    stop_component NameNode hdfs --daemon stop namenode
    ;;
  status)
    show_status
    ;;
  *)
    echo "Usage: $0 {start|stop|status} [all|hdfs]" >&2
    exit 2
    ;;
esac
