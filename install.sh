#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LICENSE_FILE="$SCRIPT_DIR/LICENSE"
LOCAL_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}"
APP_DIR="$LOCAL_ROOT/dirzero"
BIN_DIR="$HOME/.local/bin"
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICON_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps"
LICENSE_DIR="$LOCAL_ROOT/licenses/dirzero"
BUILD_DIR="$SCRIPT_DIR/build/install-linux"

fail() {
    printf 'DirZero installer: %s\n' "$*" >&2
    exit 1
}

[[ "$(uname -s)" == "Linux" ]] || fail "This installer supports Linux. Use the Windows installer on Windows."
[[ -f "$LICENSE_FILE" ]] || fail "GPLv3 license text is missing: $LICENSE_FILE"

if [[ "${DIRZERO_ACCEPT_GPL:-}" != "1" ]]; then
    printf 'DirZero is licensed under the GNU General Public License, version 3 (GPLv3).\n'
    printf 'Review the full license below. Type ACCEPT to continue with installation.\n\n'
    if [[ -t 1 ]] && command -v less >/dev/null 2>&1; then
        less "$LICENSE_FILE"
    else
        cat "$LICENSE_FILE"
    fi
    printf '\n'
    read -r -p "Do you accept the GPLv3 license? Type ACCEPT: " answer
    [[ "$answer" == "ACCEPT" ]] || fail "License was not accepted; nothing was installed."
fi

install_dependencies() {
    local manager=""
    if command -v apt-get >/dev/null 2>&1; then
        manager="apt"
    elif command -v dnf >/dev/null 2>&1; then
        manager="dnf"
    elif command -v pacman >/dev/null 2>&1; then
        manager="pacman"
    else
        fail "Unsupported Linux distribution. Install CMake, a C++17 compiler, Qt 6 Widgets/Network development files, pkg-config, and libssh development files, then rerun."
    fi

    local -a privilege=()
    if [[ "$EUID" -ne 0 ]]; then
        command -v sudo >/dev/null 2>&1 || fail "sudo is required to install build dependencies."
        privilege=(sudo)
    fi

    case "$manager" in
        apt)
            "${privilege[@]}" apt-get update
            "${privilege[@]}" apt-get install -y build-essential cmake pkg-config qt6-base-dev libssh-dev
            ;;
        dnf)
            "${privilege[@]}" dnf install -y gcc-c++ cmake pkgconf-pkg-config qt6-qtbase-devel libssh-devel
            ;;
        pacman)
            "${privilege[@]}" pacman -S --needed --noconfirm base-devel cmake pkgconf qt6-base libssh
            ;;
    esac
}

if [[ "${DIRZERO_SKIP_DEPENDENCIES:-}" != "1" ]]; then
    install_dependencies
fi

for tool in cmake pkg-config; do
    command -v "$tool" >/dev/null 2>&1 || fail "Required build tool is still unavailable after dependency installation: $tool"
done

printf '\nBuilding DirZero from %s...\n' "$SCRIPT_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')"

printf '\nInstalling DirZero for the current user...\n'
cmake --install "$BUILD_DIR" --prefix "$APP_DIR"
mkdir -p "$BIN_DIR" "$DESKTOP_DIR" "$ICON_DIR" "$LICENSE_DIR"

cat > "$BIN_DIR/dirzero" <<EOF
#!/bin/sh
exec "$APP_DIR/bin/DirZero" "\$@"
EOF
chmod 0755 "$BIN_DIR/dirzero"

install -m 0644 "$SCRIPT_DIR/assets/dirzero.svg" "$ICON_DIR/dirzero.svg"
install -m 0644 "$LICENSE_FILE" "$LICENSE_DIR/LICENSE"

desktop_exec="$BIN_DIR/dirzero"
desktop_exec="${desktop_exec//\\/\\\\}"
desktop_exec="${desktop_exec//\"/\\\"}"
desktop_exec="${desktop_exec//%/%%}"
desktop_exec="${desktop_exec//&/\\&}"
desktop_exec="${desktop_exec//|/\\|}"
sed "s|@DIRZERO_EXEC@|\"$desktop_exec\"|" \
    "$SCRIPT_DIR/packaging/linux/dirzero.desktop" > "$DESKTOP_DIR/dirzero.desktop"
chmod 0644 "$DESKTOP_DIR/dirzero.desktop"

profile_file="$HOME/.profile"
path_marker="# DirZero user-local applications"
if ! grep -Fq "$path_marker" "$profile_file" 2>/dev/null; then
    {
        printf '\n%s\n' "$path_marker"
        printf 'case ":$PATH:" in *:"$HOME/.local/bin":*) ;; *) PATH="$HOME/.local/bin:$PATH" ;; esac\n'
        printf 'export PATH\n'
    } >> "$profile_file"
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    if ! update-desktop-database "$DESKTOP_DIR"; then
        printf 'Warning: unable to update the desktop application cache.\n' >&2
    fi
fi

printf '\nDirZero is installed.\n'
printf '  App:     %s\n' "$APP_DIR/bin/DirZero"
printf '  Launcher: %s\n' "$DESKTOP_DIR/dirzero.desktop"
printf 'Launch it from your application menu, or run: %s/dirzero\n' "$BIN_DIR"
printf 'Open a new terminal to pick up the ~/.local/bin PATH update.\n'
