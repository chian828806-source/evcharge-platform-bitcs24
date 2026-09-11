#!/usr/bin/env bash
# 功能：为演示视频准备并启动 Qt 用户端，同时在当前终端显示精简的 Network 日志。
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_root="${EVCHARGE_BUILD_DIR:-$(dirname "$project_root")/build-evcharge-demo-cf1975e}"
user_binary="$build_root/qt-user/evcharge-user"
server_binary="$build_root/qt-server/evcharge-qt-server"
database_file="$project_root/database/evcharge.db"
started_server_pid=""

if [[ ! -x "$user_binary" || ! -x "$server_binary" ]]; then
    echo "[DEMO] 首次准备构建目录……"
    mkdir -p "$build_root"
    qmake6 -o "$build_root/Makefile" "$project_root/evcharge-platform.pro"
    make -C "$build_root/qt-server" -j2
    make -C "$build_root/qt-user" -j2
else
    # 仅增量编译用户端，确保刚修改的 Network 日志进入可执行文件。
    make -C "$build_root/qt-user" -j2 >/dev/null
fi

if [[ ! -f "$database_file" ]]; then
    echo "[DEMO] 初始化演示数据库……"
    sqlite3 "$database_file" ".read $project_root/database/schema.sql" ".read $project_root/database/init_data.sql"
fi

# VS Code Remote-SSH 终端未必继承桌面变量，主动读取当前 GNOME 会话环境。
while IFS='=' read -r key value; do
    case "$key" in
        DISPLAY|WAYLAND_DISPLAY|XAUTHORITY|XDG_RUNTIME_DIR|DBUS_SESSION_BUS_ADDRESS)
            export "$key=$value"
            ;;
    esac
done < <(systemctl --user show-environment)
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

# 避免上次后台启动的用户端形成两个相同窗口。
systemctl --user stop evcharge-demo-user.service 2>/dev/null || true

# 服务端若未监听则临时启动；若已运行则直接复用。
if ! ss -ltn | grep -q ':18080 '; then
    "$server_binary" --database "$database_file" >"$build_root/server-demo.log" 2>&1 &
    started_server_pid="$!"
    for _ in {1..30}; do
        ss -ltn | grep -q ':18080 ' && break
        sleep 0.1
    done
fi

cleanup() {
    if [[ -n "$started_server_pid" ]]; then
        kill "$started_server_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Qt WebEngine自身会打印大量图形环境诊断；演示终端只保留本模块的日志。
"$user_binary" 2>&1 | grep --line-buffered '^\[USER-NET\]'
