# SSH Terminal (sshterm) — Claude Code Context

## Project Overview

This is the **sshterm** app — an SSH terminal application running on the WHY2025 conference badge (ESP32-P4, 720×720 display, ESP32-C6 for WiFi/BLE). It is one of several SDK apps built on top of **BadgeVMS**, the badge operating system.

The full firmware repository lives at the repo root. Other SDK apps in `sdk_apps/` and the BadgeVMS core in `badgevms/` are good references for platform APIs and patterns. Shared headers live in `sdk_include/`.

**Changes to BadgeVMS core must go through the upstream repository as merge requests — do not modify BadgeVMS locally.**

## Architecture

Read `ARCHITECTURE.md` before making any non-trivial change. It is the authoritative design document and must be kept up to date.

### Directory Layout

```
sdk_apps/sshterm/
├── badgevms_stubs/     # BadgeVMS API stubs for local (host) builds
├── common/             # Central app state: app_state.h
├── components/
│   ├── app_controller/ # Application lifecycle, main event loop
│   ├── common/         # Shared config (terminal_config.h) and data structures
│   ├── input_system/   # Multi-mode input routing and field management
│   ├── keyboard/       # SDL3 keyboard event processing
│   ├── renderer/       # Low-level SDL3 rendering (80×39 grid, Leggie 9×18 font)
│   ├── ssh_client/     # wolfSSH/wolfSSL low-level SSH protocol wrapper
│   ├── ssh_manager/    # High-level SSH connection lifecycle
│   ├── term/           # VT100/xterm emulation via libvterm
│   ├── test_mode/      # Terminal testing without a live SSH connection
│   └── ui_manager/     # Pure presentation layer
├── sys/                # BadgeVMS System API wrappers
├── thirdparty/         # Vendored: libvterm, wolfSSH, wolfSSL (with full source)
├── main.c              # Entry point and BadgeVMS integration
├── CMakeLists.txt      # Local (host) build configuration
└── ARCHITECTURE.md
```

### Key Design Decisions

- **Blocking I/O + threads**: async I/O did not work reliably on BadgeVMS; the SSH worker thread system must not be bypassed.
- **libvterm** for VT100 emulation with a custom renderer.
- **Component isolation**: each component owns a single responsibility; cross-component calls go through public header interfaces only.
- **Dual build**: code must compile both for host (macOS/Linux + SDL3, `SSHTERM_LOCAL_BUILD` defined) and for the ESP32-P4 hardware via ESP-IDF.

### Core State

`app_state_t` in `common/app_state.h` is the single shared context passed to every component (UI state, SSH connection info, terminal state, input mode, etc.).

## Build System

### Local (host) build — fast iteration

```bash
cd sdk_apps/sshterm
./build.sh
```

Uses SDL3 for graphics/input. Hardware functions are stubbed. `SSHTERM_LOCAL_BUILD` is defined.

### Hardware build — badge deployment

```bash
export IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.9_env
export IDF_TARGET=esp32p4
source ~/esp/v5.5/esp-idf/export.sh
# from repo root:
idf.py build flash monitor
```

**When touching CMakeLists.txt**, update both:
- `sdk_apps/sshterm/CMakeLists.txt` (local build)
- `sdk_apps/CMakeLists.txt` (hardware build)

## Development Rules

### Before starting any change

1. Read `ARCHITECTURE.md` fully.
2. Identify which component(s) own the relevant responsibility.
3. Agree on the approach before writing any code — plan first, then implement.
4. Never cross component boundaries; if new logic doesn't fit an existing component, create a new one.

### Code standards

- Use BadgeVMS APIs for all hardware access (crypto, display, WiFi) — never direct ESP-IDF calls.
- Whatever you implement, ensure that it will work within the constrained BadgeVMS ecosystem.
- Maintain dual-build compatibility; guard hardware-only code with `#ifndef SSHTERM_LOCAL_BUILD`.
- Robust error handling and user-visible feedback for network operations.
- Keep component interfaces (public headers) minimal and well-defined.
- Update `ARCHITECTURE.md` for any architectural change.

### Working files — never commit

- `TODO.md` — living log of open issues and implementation status. Read it at the start of a session; update it as work lands. **Never stage or commit it.**

### Testing

- **Local**: `./build.sh` then run the binary — use `test_mode` to exercise terminal rendering without SSH.
- **Hardware**: `idf.py build flash monitor`.
- Verify each change before moving to the next.

## Implementation Status

| Area | Status |
|---|---|
| VT100 emulation | Done |
| SSH password auth | Done |
| Basic rendering | Done |
| UI navigation | Done |
| Connection management | Done |
| Local + hardware builds | Done |
| BadgeVMS integration | Done |
| SSH keyboard-interactive auth | Done |
| UI/UX improvements | Done |
| Hardware RNG (needs BadgeVMS update) | Blocked |
| Host key verification / TOFU | Not started |
| Public key auth | Not started |
| Full charset / keyboard mapping | Not started |
| Hardware crypto offloading | Not started |
| Rendering bugs (htop, cmatrix, etc.) | Open |
