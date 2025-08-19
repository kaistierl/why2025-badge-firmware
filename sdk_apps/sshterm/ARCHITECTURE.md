# SSH Terminal for WHY 2025 Badge — Minimal Architecture Overview

**Target device:** ESP32-P4 SoC + ESP32-C6 (Wi-Fi/BLE carrier)
**Display:** 720×720 px
**Keyboard:** Custom badge keyboard (see image); limited function keys, Page Up/Down supported via software mapping
**OS/Platform:** BadgeVMS (SDL3 abstraction over hardware)
**License alignment:** Badge base firmware is **GPLv3** → all dependencies below are GPLv3-compatible.

---

## 1. Purpose & Scope

This document defines the **minimal viable architecture** for a terminal application with SSH support running on the MCH2025 badge.

**MVP Goals**

* Show a working VT100/xterm-style terminal (basic text, colors, cursor).
* Connect to a remote host over SSH with password authentication.
* Render fitting on 720×720 with fixed grid.
* Keep alive over Wi-Fi with simple reconnect logic.

**Non-Goals (for now)**

* Public-key authentication or key storage.
* Scrollback viewer, copy/paste, or OSC52 clipboard.
* Tabs, multiple sessions, or SFTP.
* Mouse reporting or advanced Unicode.
* Configuration files and settings UI.

---

## 2. High-Level Design

```
┌─────────────────────────────────────────────────────────┐
│                          App Core                       │
│                                                         │
│  ┌─────────────┐    UTF-8 bytes    ┌─────────────┐      │
│  │   SSH Client│◄─────────────────►│  Terminal   │      │
│  │ (libssh2 +  │                   │ (libvterm)  │      │
│  │  mbedTLS)   │    bytes          └──────┬──────┘      │
│  └─────┬───────┘                        screen diffs    │
│        │ socket                          │              │
│        ▼                                 ▼              │
│   lwIP/ESP-IDF sockets             ┌─────────────┐      │
│                                    │  Renderer   │      │
│ Keyboard ─► Input Mapper ─► keys   │ (SDL3 grid) │─► FB │
│                                    └─────────────┘      │
│                                                         │
│ Storage: known_hosts only (flash FS via BadgeVMS)       │
└─────────────────────────────────────────────────────────┘
```

**Concurrency model:** Single-threaded event loop (SDL events + `poll()` on socket).

---

## 3. Dependencies (GPLv3-compatible)

* **Terminal:** `libvterm-0.3.3` (MIT)
* **SSH:** `libssh2` (BSD-2-Clause)
* **TLS/Crypto:** `mbedTLS 3.x` (Apache-2.0, GPLv3-compatible)
* **Rendering:** `SDL3` (zlib) via BadgeVMS
* **Network:** ESP-IDF sockets (lwIP)

---

## 4. Display & Grid

* **Default font:** Leggie **9×18** bitmap.
* **Default grid:** **80×39** (720/9 = 80 cols; 702/18 = 39 rows; padding top/bottom).
* `TERM=xterm-256color`, `COLORTERM=truecolor`.

---

## 5. Modules

### 5.1 Renderer (SDL3, optimized)

* **Grid layout:** Fixed 80×39 grid with Leggie 9×18 font and 9px top padding.
* **Performance:** Dirty flag optimization — only redraws when screen content or cursor changes.
* **Font support:** Printable ASCII (32–126) with run-length encoded glyph rendering.  
  Other codepoints are rendered as space.
* **Color support:** Full 24-bit RGB foreground and background colors.
* **Cursor:** Underline style, 500ms blink cycle with proper visibility control.
* **Scrolling:** Efficient memmove-based scroll operations for terminal output.
* **Architecture:** Separation of screen buffer and rendering for optimal performance.

### 5.2 Terminal Emulator (libvterm-0.3.3)

* **Emulation:** Complete VT100/xterm sequence support with libvterm-0.3.3.
* **Text support:** Full UTF-8 input and display with proper multi-byte character handling.
* **Color modes:** Both 24-bit RGB truecolor and 8-color ANSI indexed color support.
* **Text attributes:** Bold text rendering with automatic brightness enhancement.
* **Input processing:** Comprehensive keyboard mapping including:
  * Arrow keys (Up/Down/Left/Right)
  * Navigation keys (Home/End) 
  * Control keys (Enter/Tab/Escape/Backspace)
  * Modifier combinations (Ctrl/Alt/Shift)
  * Alt+letter escape sequence generation
* **Screen management:** Damage-based rendering optimization with libvterm callbacks.
* **Architecture:** Clean separation between terminal emulation (libvterm) and display (renderer).
* **Limitations:** No OSC52 clipboard, no bracketed paste, no mouse support.
* **Scrollback:** Uses libvterm's built-in scrollback (~100 lines); no external scrollback store.

### 5.3 SSH Engine (libssh2 + mbedTLS)

* **Auth:** Password only (prompt on connect).
* **Crypto:** curve25519-sha256 + chacha20-poly1305 preferred; AES-GCM fallback.
* **Host key:** TOFU — show SHA-256 fingerprint on first connect; store in `known_hosts`.
* **Environment setup:** Sets TERM=xterm-256color, COLORTERM=truecolor, COLUMNS=80, LINES=39.
* **Keepalive:** rely on TCP keepalive only.
* Single profile compiled into firmware (host, port, username). No JSON config.

### 5.4 Keyboard Input (SDL3 events)

* **Architecture:** Event-driven input processing with generic modifier key system.
* **Key categories:**
  * Printable characters handled via SDL_EVENT_TEXT_INPUT for proper UTF-8.
  * Control and navigation keys mapped to VT100/xterm escape sequences.
  * Modifier combinations (Ctrl/Alt/Shift) processed with precedence rules.
* **Special handling:**
  * Alt + letter combinations have precedence over Ctrl for letters.
  * All navigation keys support modifier combinations for advanced terminal navigation.
  * Ctrl+Q reserved for application exit (not sent to terminal).

### 5.5 Input Mapper

* Maps essential keys for terminal operation:

  * **Characters:** All printable ASCII via SDL_EVENT_TEXT_INPUT.
  * **Control keys:** Enter, Backspace (0x7f), Tab, Esc, Delete (127).
  * **Navigation:** Arrows (`\x1b[A/B/C/D`), Home (`\x1b[H`), End (`\x1b[F`).
  * **Extended navigation:** Insert (`\x1b[2~`), Page Up (`\x1b[5~`), Page Down (`\x1b[6~`).
  * **Modifier combinations:**
    * Ctrl + A-Z: Standard control characters (0x01-0x1A).
    * Alt + A-Z: ESC prefix sequences (e.g., Alt+A sends `\x1b` + `a`).
    * All navigation keys support Ctrl/Alt/Shift modifiers.
  * **Application control:** Ctrl+Q exits the terminal application.

* **Not supported:** Function keys (F1-F12), compose sequences, IME.
* **Architecture:** Generic modifier system allows easy extension for additional key combinations.

---

## 6. Storage

* `known_hosts` (OpenSSH format).
* No key files, no config.json.
* If flash FS unavailable, TOFU re-asks each session.

---

## 7. Component Interfaces

The application is built with clean component separation through well-defined interfaces:

* **Renderer:** Provides grid-based terminal display with color support and cursor management
* **Terminal:** Handles VT100/xterm emulation and UTF-8 processing via libvterm
* **Keyboard:** Converts SDL input events to terminal key sequences  
* **SSH Client:** Manages SSH connections and data transfer (planned)

All interfaces are defined in their respective header files (`renderer.h`, `term.h`, `keyboard.h`, `ssh_client.h`).

---

## 8. Security

* TOFU host key check with SHA-256 fingerprint.
* Only password auth (no key mgmt yet).
* Minimal cipher suite; disable legacy ciphers/MD5/SHA-1.
* No agent forwarding, no port/X11 forwarding.

---

## 9. Build & Layout

```
/app
  /components
    /keyboard
    /renderer
    /term
    /ssh
  /thirdparty
    /libvterm
    /libssh2
    /mbedtls
  /main
    main.c
  ARCHITECTURE.md
```

---

## 10. Error Handling & Recovery

* On Wi-Fi drop / SSH error: clear screen, show message, prompt `[Enter] to reconnect`.
* No exponential backoff or fancy UI.

---

## 11. Roadmap

**MVP (current implementation)**

* Fixed 80×39 grid with Leggie 9×18 font and optimized dirty-flag rendering.
* Complete VT100/xterm terminal emulation with libvterm-0.3.3.
* Full UTF-8 support with proper multi-byte character handling.
* 24-bit RGB truecolor and 8-color ANSI indexed color support.
* Bold text rendering with brightness enhancement.
* Comprehensive keyboard input mapping (arrows, home/end, modifiers).
* Damage-based screen updates with cursor blinking.
* Generic modifier key system supporting Ctrl/Alt combinations.
* Password-auth SSH with TOFU host key (planned).
* Reconnect loop (planned).
* Soft-Key mapping for keys that are not physically present (Pg-Up / Pg-Down / Home / End / F-Keys etc.) (planned)

**Later**

* Text rendering enhancements (bold, underlined, reversed, full UTF-8 font)
* Public-key auth + key store.
* External scrollback viewer.
* Themes, status bar, soft key mappings.
* OSC52 clipboard, bracketed paste.
* Multiple sessions, SFTP.

---

## 12. Licensing

This application is licensed under GPLv3. See `LICENSE` in this directory for the summary and the repository root `COPYING` for the full license text. Third-party components and required attributions (including the Leggie font, libvterm, and SDL3) are listed in `THIRD_PARTY_NOTICES.md`.