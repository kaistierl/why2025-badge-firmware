# SSH Terminal - Open Issues

Issues are prioritized by severity. Items resolved in the commit history have
been removed.

---

## 🔴 Critical Security Issues (Must Fix Before Production)

### 1. **Hardware TRNG Not Yet Used** - HIGH
**File:** `components/ssh_client/badge_crypto_port.c`
**Issue:** The ESP32-P4 hardware TRNG is not accessible through BadgeVMS APIs yet.
The software fallback now uses a SHA-256 entropy pool seeded from WiFi RSSI,
BME690 sensor readings (temperature, humidity, pressure, gas resistance),
wall-clock time, and device MAC. Local builds use `/dev/urandom`.

**Remaining risk:** Software entropy sources have lower bit-rate than the TRNG.
Session keys are significantly harder to predict than with the old LCG fallback,
but not as strong as hardware RNG.

**Blocked on:** BadgeVMS exposing the ESP32-P4 TRNG. Once available, replace
`reseed_pool()` in `badge_crypto_port.c` with the BadgeVMS TRNG API call.

---

### 2. **No SSH Host Key Verification** - CRITICAL
**File:** `components/ssh_client/ssh_client.c:155` (`ssh_public_key_check`)
**Issue:** Callback unconditionally returns `WS_SUCCESS` — all server keys
accepted with no check.

**Security Impact:** No protection against man-in-the-middle attacks.

**Solution:** Implement TOFU (Trust On First Use):
1. Display key fingerprint; prompt user on first connection.
2. Store accepted keys in NVS flash partition.
3. Warn / block on fingerprint mismatch.

---

## 🟡 Medium Priority

### 3. **NVS Storage Layer** — prerequisite for items #2 and #8
**File:** New component `storage/` or `components/storage/`
**Issue:** No persistent storage exists yet. BadgeVMS flash filesystem needs a thin
wrapper before host keys, connection history, or any config can survive reboots.

**Work:**
1. Implement a minimal key–value store backed by BadgeVMS flash filesystem.
2. Design a stable on-disk layout for `known_hosts` and connection records.
3. Guard all NVS calls with `#ifndef SSHTERM_LOCAL_BUILD` stubs for local builds.

---

### 4. **Rendering Bugs (ncurses/full-screen apps)** — affects daily usability
**Files:** `components/term/`, `components/renderer/`
**Issue:** Apps like `htop`, `cmatrix`, and others using alternate screen or complex
cursor/erase sequences do not render correctly. Specific escape sequences not yet
diagnosed.

**Work:**
1. Identify broken sequences by running suspect apps with `script(1)` capture and
   replaying through `test_mode`.
2. Fix libvterm callback coverage or renderer cell-update logic as needed.
3. Extend `test_mode` with replay-from-file capability to prevent regressions.

---

### 5. **Connection Keep-alive & Auto-reconnect**
**Files:** `components/ssh_manager/ssh_thread.c`, `ssh_manager.c`
**Issue:** Badge Wi-Fi can drop silently. A dead connection hangs the I/O threads
indefinitely with no feedback or recovery.

**Work:**
1. Enable wolfSSH keepalive (`WOLFSSH_KEEPALIVE`) with a configurable interval.
2. Detect socket EOF / repeated EAGAIN from blocking recv as a disconnection event.
3. Surface a "connection lost" status and offer immediate retry (reusing last params).

---

### 6. **Scrollback Viewer**
**Files:** `components/term/`, `components/input_system/`, `components/renderer/`
**Issue:** libvterm already maintains ~100 lines of scrollback, but there is no way
to view it. Page Up / Page Down are mapped to SSH but do nothing useful when the
remote side does not handle them.

**Work:**
1. Add a `SCROLLBACK` input mode that intercepts Page Up/Down.
2. Expose libvterm's `vterm_screen_get_cell` at a scroll offset.
3. Draw a faint scroll indicator in the status line; exit scrollback on any keypress.

---

### 7. **Soft-key Mapping Interface**
**Files:** `components/keyboard/`, `components/input_system/`
**Issue:** The badge keyboard lacks F1–F12, Insert, and other keys common in terminal
apps. Users have no way to send them.

**Work:**
1. Define a key-chord (e.g. Badge-menu + letter) that opens a soft-key overlay.
2. Render a small on-screen picker listing F-key names → VT sequences.
3. Inject the selected sequence into the terminal as if typed.

---

## 🟢 Low Priority

### 8. **Public Key Authentication**
**Files:** `components/ssh_client/ssh_client.c`, new `storage/` component
**Depends on:** Item #3 (NVS storage layer)
**Issue:** Only password and keyboard-interactive auth are supported. Public key auth
is more convenient and eliminates password exposure.

**Work:**
1. Generate or import an Ed25519 key pair; store private key in NVS (encrypted if
   BadgeVMS exposes an AES key-wrap API).
2. Expose key fingerprint in the connection UI for out-of-band authorisation.
3. Add `pubkey` to the auth method list in `ssh_client.c`.

---

### 9. ~~**Software Entropy Pool Improvement**~~ — **DONE**

Replaced three-LCG fallback with a SHA-256 entropy pool seeded from WiFi RSSI,
BME690 sensor readings, wall-clock time, and device MAC. Local builds use
`/dev/urandom`. Output is expanded via counter-mode SHA-256. See item #1 for the
remaining hardware TRNG TODO.

---

### 10. **Connection History**
**Files:** New `storage/` component, `components/ui_manager/`, `components/input_system/`
**Depends on:** Item #3 (NVS storage layer)
**Issue:** Users must retype hostname, username, and port on every boot.

**Work:**
1. Persist up to ~5 recent `connection_input_t` records in NVS (no passwords).
2. Add a pre-connection menu that lets users pick a saved entry or enter new params.

---

### 11. **Bracketed Paste Mode**
**Files:** `components/term/`, `components/keyboard/`
**Issue:** Pasting multi-line text into editors (vim, nano) causes auto-indentation
chaos because the terminal does not send bracketed-paste markers.

**Work:**
1. Track `?2004h` / `?2004l` DEC private mode in the libvterm state machine.
2. Wrap SDL clipboard content in `\e[200~` … `\e[201~` when the mode is active.

---

### 12. **Extended Character Renderer**
**Files:** `components/renderer/`
**Issue:** Renderer only handles printable ASCII (32–126); extended Latin, box-drawing
characters, and other common codepoints are rendered as spaces, breaking many TUI apps.

**Work:**
1. Audit which Unicode blocks matter most (box-drawing U+2500–U+257F, Latin-1 supplement).
2. Add corresponding glyphs to the Leggie bitmap font or embed a supplementary glyph table.
3. Update cell rendering to look up multi-byte codepoints.
