# bb-auth

Unified authentication daemon.

- Polkit authentication agent
- GNOME Keyring system prompter replacement
- GPG pinentry bridge

This daemon is consumed by the `bb-auth` shell plugin.

UI providers (plugins) connect over the local socket and handle authentication prompts. The daemon launches `bb-auth-fallback` automatically when no provider is active.

## Runtime Contract

- Service: `bb-auth.service` (user service, Wayland-only by default)
- Socket: `$XDG_RUNTIME_DIR/bb-auth.sock`
- D-Bus: `org.bb.auth` (polkit agent), `org.gnome.keyring.SystemPrompter` (keyring prompter)
- Main binary: `bb-auth`
- Pinentry binary: `pinentry-bb` (symlink to `bb-auth`)
- Fallback UI binary: `bb-auth-fallback`

## Install Locations

Binaries install to `libexecdir` (typically `/usr/libexec` or `~/.local/libexec`):
- `/usr/libexec/bb-auth` (main daemon)
- `/usr/libexec/bb-auth-fallback` (fallback UI)
- `/usr/libexec/pinentry-bb` → `bb-auth` (symlink)
- `/usr/libexec/bb-auth-bootstrap` (service setup)

User-facing command (in `PATH`):
- `/usr/bin/bb-auth-migrate` (migration script from noctalia-auth)

Systemd unit:
- `/usr/lib/systemd/user/bb-auth.service`

D-Bus service files:
- `/usr/share/dbus-1/services/org.bb.auth.service`
- `~/.local/share/dbus-1/services/org.gnome.keyring.SystemPrompter.service` (installed at runtime)

## Install

### AUR

```bash
yay -S bb-auth-git
```

### Manual build

Dependencies (distro names vary): Qt6 base, polkit-qt6, polkit, gcr-4, json-glib, cmake, pkg-config.

```bash
git clone https://github.com/anthonyhab/bb-auth
cd bb-auth
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

The systemd unit requires `WAYLAND_DISPLAY` (Wayland-only). To run on X11, remove or override the `ConditionEnvironment=WAYLAND_DISPLAY` line in the service file.

Enable the service:

```bash
systemctl --user daemon-reload
systemctl --user enable --now bb-auth.service
```

On each service start, bootstrap automatically:

- validates and repairs `gpg-agent` pinentry path when stale
- stops known competing polkit agents for the current session

If no shell UI provider is active when an auth request arrives, daemon launches `bb-auth-fallback` automatically.
The fallback app enforces a single-instance lock and stands down when a higher-priority shell provider becomes active.

For tiling compositors, the fallback app uses a normal top-level window so it can still be tiled manually by the compositor.
**Hyprland** (0.53+):
```ini
windowrule = float, class:^(bb-auth-fallback)$
windowrule = center, class:^(bb-auth-fallback)$
windowrule = size 560 360, class:^(bb-auth-fallback)$
```

**Niri**:
```kdl
window-rule {
    match app-id="bb-auth-fallback"
    default-column-width { fixed 560; }
    default-window-height { fixed 360; }
    open-floating true
    center-on-cursor true
}
```

**Sway / i3**:
```
for_window [app_id="bb-auth-fallback"] floating enable, resize set 560 360, move position center
```

## Development workflow

```bash
./build-dev.sh install
./build-dev.sh enable
./build-dev.sh doctor
```

Useful commands:

```bash
./build-dev.sh status
./build-dev.sh disable
./build-dev.sh uninstall
```

## Conflict policy

Default policy is `session` (Linux-safe best practice): competing agents are stopped for the current session only.

Optional modes can be set with a service override environment variable:

- `BB_AUTH_CONFLICT_MODE=session` (default)
- `BB_AUTH_CONFLICT_MODE=persistent` (disable known competing user services/autostarts)
- `BB_AUTH_CONFLICT_MODE=warn` (detect only)

In `persistent` mode, user autostart entries are backed up under `~/.local/state/bb-auth/autostart-backups/` before override files are written.

Example:

```bash
systemctl --user edit bb-auth.service
```

Add:

```ini
[Service]
Environment=BB_AUTH_CONFLICT_MODE=session
```

## Smoke checks

```bash
pkexec true
echo test | secret-tool store --label=test attr val
```

If prompts do not appear, run `./build-dev.sh doctor` first.

For common failures, see `docs/TROUBLESHOOTING.md`.

## Nix

```bash
nix build .#bb-auth
nix profile install .#bb-auth
```

## Rebrand Notice

Previously known as `noctalia-auth` (daemon) and `polkit-auth` (plugin).  
Renamed to BB Auth as part of the "habibe → bibe → BB" branding.  

### Upgrading from noctalia-auth
If you were using the previous version, run the migration script after installing:

```bash
bb-auth-migrate
# Or to automatically remove legacy binaries:
bb-auth-migrate --remove-binaries
```

This will:
- Stop and disable the old `noctalia-auth.service`
- Clean up legacy runtime files and state
- Back up your old state directory
- Report any legacy binaries that need manual removal
- **Enable and start `bb-auth.service` automatically** (if bb-auth is installed)

If bb-auth is not installed, the script will tell you exactly how to install it
for your platform (AUR, Nix, or source build).

To perform cleanup without enabling the service:

```bash
bb-auth-migrate --no-enable
```

The plugin ID has changed from `polkit-auth` to `bb-auth`. Reinstall through your shell's plugin UI.
