# noctalia-polkit
A polkit authentication agent and GNOME Keyring prompter for [noctalia-shell](https://github.com/noctalia-dev/noctalia-shell).

Requires polkit-auth [plugin](https://github.com/anthonyhab/noctalia-plugins/tree/main/polkit-auth).

## Features
- Polkit authentication prompts
- GNOME Keyring unlock prompts (replaces `gcr-prompter`)


## Install

### Arch Linux (AUR)

```bash
yay -S noctalia-polkit-git
```

### Manual Build (Arch / Fedora / Debian/Ubuntu)

Dependencies (names vary by distro): Qt6 base, polkit-qt6, polkit, gcr-4, json-glib, cmake, pkg-config.

```bash
git clone https://github.com/anthonyhab/noctalia-polkit/
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

Enable the user service:

```bash
systemctl --user daemon-reload
systemctl --user enable --now noctalia-polkit.service
```

Notes:
- Test polkit authentication by running `pkexec true`
- Test keyring unlock by locking your keyring (via Seahorse or `dbus-send`) then running `echo test | secret-tool store --label=test attr val`
- Make sure you have the plugin installed, reload noctalia-shell if you aren't seeing requests after installing.
- The Noctalia plugin connects over IPC at `$XDG_RUNTIME_DIR/noctalia-polkit-agent.sock`.

### NixOS / Nix

```bash
nix build .#noctalia-polkit
# or install into your user profile:
nix profile install .#noctalia-polkit
```

Then enable the user service:

```bash
systemctl --user daemon-reload
systemctl --user enable --now noctalia-polkit.service
```
