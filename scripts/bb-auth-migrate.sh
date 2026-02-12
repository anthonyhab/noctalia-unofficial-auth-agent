#!/usr/bin/env bash
# BB Auth Migration Script
# Run this once when upgrading from noctalia-auth/polkit-auth to bb-auth
# This script will be removed in a future release

set -euo pipefail
SCRIPT_NAME="bb-auth-migrate"
LEGACY_SERVICE="noctalia-auth.service"
NEW_SERVICE="bb-auth.service"

# Parse arguments
REMOVE_BINARIES=false
NO_ENABLE=false

for arg in "$@"; do
    case "$arg" in
        --remove-binaries) REMOVE_BINARIES=true ;;
        --no-enable) NO_ENABLE=true ;;
        --help)
            echo "Usage: $SCRIPT_NAME [--remove-binaries] [--no-enable]"
            echo "  --remove-binaries  Automatically remove legacy binaries"
            echo "  --no-enable        Skip enabling/starting bb-auth.service"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            echo "Usage: $SCRIPT_NAME [--remove-binaries] [--no-enable]" >&2
            exit 1
            ;;
    esac
done

log() {
    printf '[%s] %s\n' "$SCRIPT_NAME" "$*"
}

log "Starting migration from noctalia-auth to bb-auth"

# 1. Stop and disable legacy service
if systemctl --user is-active "$LEGACY_SERVICE" &>/dev/null; then
    log "Stopping $LEGACY_SERVICE..."
    systemctl --user stop "$LEGACY_SERVICE" || true
fi

if systemctl --user is-enabled "$LEGACY_SERVICE" &>/dev/null; then
    log "Disabling $LEGACY_SERVICE..."
    systemctl --user disable "$LEGACY_SERVICE" || true
fi

# 2. Remove user systemd service overrides
LEGACY_OVERRIDE_DIR="$HOME/.config/systemd/user/noctalia-auth.service.d"
if [ -d "$LEGACY_OVERRIDE_DIR" ]; then
    log "Removing legacy service overrides..."
    rm -rf "$LEGACY_OVERRIDE_DIR"
fi

# 3. Clean up legacy runtime files
LEGACY_SOCKET="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/noctalia-auth.sock"
if [ -e "$LEGACY_SOCKET" ]; then
    log "Removing legacy socket..."
    rm -f "$LEGACY_SOCKET"
fi

LEGACY_LOCK="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/noctalia-auth-fallback.lock"
if [ -e "$LEGACY_LOCK" ]; then
    log "Removing legacy lock file..."
    rm -f "$LEGACY_LOCK"
fi

# 4. Backup and clean legacy state
LEGACY_STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/noctalia-auth"
if [ -d "$LEGACY_STATE_DIR" ]; then
    BACKUP_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/noctalia-auth-backup-$(date +%Y%m%d-%H%M%S)"
    log "Backing up legacy state to $BACKUP_DIR..."
    cp -r "$LEGACY_STATE_DIR" "$BACKUP_DIR"
    log "Removing legacy state directory..."
    rm -rf "$LEGACY_STATE_DIR"
fi

# 5. Find and report legacy binaries
LEGACY_BINARIES=(
    "$HOME/.local/libexec/noctalia-auth"
    "$HOME/.local/libexec/noctalia-auth-fallback"
    "$HOME/.local/libexec/noctalia-auth-bootstrap"
    "$HOME/.local/libexec/noctalia-keyring-prompter"
    "$HOME/.local/libexec/pinentry-noctalia"
    "$HOME/.local/libexec/noctalia-polkit"  # Very old
)

FOUND_LEGACY=()
for binary in "${LEGACY_BINARIES[@]}"; do
    if [ -e "$binary" ]; then
        FOUND_LEGACY+=("$binary")
    fi
done

if [ ${#FOUND_LEGACY[@]} -gt 0 ]; then
    log "Found legacy binaries that should be manually removed:"
    for binary in "${FOUND_LEGACY[@]}"; do
        echo "  - $binary"
    done
    echo ""
    log "To remove them automatically, run:"
    echo "  bb-auth-migrate --remove-binaries"
fi

# 6. Check for legacy D-Bus service files
LEGACY_DBUS_SERVICE="$HOME/.local/share/dbus-1/services/org.noctalia.polkitagent.service"
if [ -e "$LEGACY_DBUS_SERVICE" ]; then
    log "Removing legacy D-Bus service file..."
    rm -f "$LEGACY_DBUS_SERVICE"
fi

# 7. Check for autostart overrides that reference old names
AUTOSTART_DIR="$HOME/.config/autostart"
if [ -d "$AUTOSTART_DIR" ]; then
    for file in "$AUTOSTART_DIR"/*.desktop; do
        if [ -f "$file" ] && grep -q "noctalia-auth\|polkit-auth" "$file" 2>/dev/null; then
            log "Found legacy reference in: $file"
            log "You may want to review and update this file manually"
        fi
    done
fi

# Handle --remove-binaries flag
if [ "$REMOVE_BINARIES" = true ] && [ ${#FOUND_LEGACY[@]} -gt 0 ]; then
    log "Removing legacy binaries..."
    for binary in "${FOUND_LEGACY[@]}"; do
        rm -f "$binary"
        log "Removed: $binary"
    done
fi

# 8. Reload systemd daemon
log "Reloading systemd daemon..."
if ! systemctl --user daemon-reload 2>/dev/null; then
    log "WARNING: Failed to reload systemd daemon. User manager may not be running."
    log "         You may need to log out and back in, then run:"
    log "         systemctl --user daemon-reload"
fi

# 9. Enable and start new service (if installed and not disabled)
BB_AUTH_INSTALLED=false
if systemctl --user cat "$NEW_SERVICE" &>/dev/null; then
    BB_AUTH_INSTALLED=true
fi

if [ "$BB_AUTH_INSTALLED" = false ]; then
    log ""
    log "=========================================="
    log "bb-auth is NOT installed"
    log "=========================================="
    log ""
    log "The migration script cleaned up legacy files, but bb-auth"
    log "itself is not installed (service unit not found)."
    log ""
    log "Install bb-auth via one of:"
    log ""
    log "  AUR (Arch):"
    log "    yay -S bb-auth-git"
    log ""
    log "  Nix:"
    log "    nix profile install .#bb-auth"
    log ""
    log "  Source build:"
    log "    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr"
    log "    cmake --build build"
    log "    sudo cmake --install build"
    log ""
    log "After installing, either re-run this script or enable manually:"
    log "    systemctl --user enable --now bb-auth.service"
    log ""
    exit 0
fi

# bb-auth is installed
if [ "$NO_ENABLE" = true ]; then
    log ""
    log "=========================================="
    log "Migration complete!"
    log "=========================================="
    log ""
    log "bb-auth is installed (--no-enable specified, service not started)"
    log ""
    log "To enable/start manually:"
    log "    systemctl --user enable --now bb-auth.service"
    log ""
else
    log "Enabling and starting bb-auth.service..."
    if systemctl --user is-enabled "$NEW_SERVICE" &>/dev/null; then
        # Already enabled, just restart
        if ! systemctl --user restart "$NEW_SERVICE" 2>/dev/null; then
            log "WARNING: Failed to restart bb-auth.service"
            log "         Check status with: systemctl --user status bb-auth.service"
        fi
    else
        # Not enabled, enable and start
        if ! systemctl --user enable --now "$NEW_SERVICE" 2>/dev/null; then
            log "WARNING: Failed to enable/start bb-auth.service"
            log "         Check status with: systemctl --user status bb-auth.service"
        fi
    fi

    # Verify service status
    sleep 0.5
    if systemctl --user is-active "$NEW_SERVICE" &>/dev/null; then
        log "bb-auth.service is active"
    else
        log "WARNING: bb-auth.service is not active"
        log "         Check: systemctl --user status bb-auth.service"
    fi

    log ""
    log "=========================================="
    log "Migration complete!"
    log "=========================================="
    log ""
fi
log "Next steps:"
log "  - Update your shell plugin: remove 'polkit-auth', install 'bb-auth'"
log "  - Test: pkexec true"
log ""
if [ -d "$LEGACY_STATE_DIR" ] 2>/dev/null; then
    log "Note: Legacy state backed up to: $BACKUP_DIR"
    log ""
fi