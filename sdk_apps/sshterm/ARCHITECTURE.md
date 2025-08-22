# SSH Terminal for WHY 2025 Badge — Architecture Overview

**Target device:** ESP32-P4 SoC + ESP32-C6 (Wi-Fi/BLE carrier)
**Display:** 720×720 px
**Keyboard:** Custom badge keyboard (see image); limited function keys, Page Up/Down supported via software mapping
**OS/Platform:** BadgeVMS (SDL3 abstraction over hardware)
**License alignment:** Badge base firmware is **GPLv3** → all dependencies below are GPLv3-compatible.

---

## 1. Purpose & Scope

This document defines the architecture for a terminal application with SSH support running on the WHY 2025 badge.

**Implemented Features**

* Complete VT100/xterm-style terminal emulation with libvterm-0.3.3
* SSH connections to remote hosts with password authentication via libssh2
* 720×720 display rendering with optimized dirty-flag system
* Interactive SSH connection setup with field-by-field input
* Terminal test mode for feature validation
* Clean component separation with well-defined interfaces
* Comprehensive input handling including special keys and modifiers

**Current Non-Goals**

* Public-key authentication or key storage
* Scrollback viewer, copy/paste, or OSC52 clipboard
* Tabs, multiple sessions, or SFTP
* Mouse reporting or advanced Unicode fonts
* Configuration files and persistent settings or advances settings UI

---

## 2. High-Level Design

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Application Controller                             │
│                    (Main loop, event handling, coordination)                │
│                                                                             │
│  ┌──────────────┐    ┌─────────────┐    ┌──────────────┐    ┌────────────┐  │
│  │ SSH Manager  │    │ Input System│    │ UI Manager   │    │ Test Mode  │  │
│  │ (Connection  │◄──►│ (Input mode │◄──►│ (UI displays │    │ (Terminal  │  │
│  │  management) │    │  routing)   │    │  & prompts)  │    │  testing)  │  │
│  └──────┬───────┘    └─────────────┘    └──────┬───────┘    └────────────┘  │
│         │                                      │                            │
│  ┌──────▼───────┐    UTF-8 bytes        ┌──────▼──────┐                     │
│  │  SSH Client  │◄─────────────────────►│  Terminal   │                     │
│  │ (libssh2 +   │                       │ (libvterm)  │                     │
│  │  mbedTLS)    │    bytes              └──────┬──────┘                     │
│  └──────┬───────┘                              │screen diffs                │
│         │ socket                               ▼                            │
│         ▼                                 ┌─────────────┐                   │
│   lwIP/POSIX sockets                      │  Renderer   │                   │
│                                           │ (SDL3 grid) │─► Framebuffer     │
│ Keyboard ─► Input ─► Key sequences  ──────┤             │                   │
│            System                         └─────────────┘                   │
│                                                                             │
│ App State: SSH connection status, input mode, connection parameters         │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Concurrency model:** Single-threaded event loop (SDL events + non-blocking SSH I/O).

**Component Architecture:** Clean separation with well-defined interfaces:
- **App Controller:** Main application lifecycle and component coordination
- **SSH Manager:** High-level SSH connection business logic and state management
- **SSH Client:** Low-level SSH protocol implementation (libssh2 wrapper)
- **Input System:** Input mode routing and field management for different app states
- **UI Manager:** User interface rendering and prompt management
- **Terminal/Renderer:** VT100 emulation and optimized display rendering
- **Test Mode:** Standalone terminal testing without SSH connection

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

### 5.1 Application Controller (`app_controller/`)

* **Responsibility:** Main application lifecycle, event loop coordination, and component integration
* **Architecture:** Single-threaded SDL event loop with non-blocking I/O for SSH operations
* **Key features:**
  * SDL window and renderer initialization
  * Event routing between input system, SSH manager, and UI manager  
  * Application state management and graceful shutdown handling
* **Interface:** Clean separation between initialization, main loop execution, and cleanup phases

### 5.2 Renderer (`renderer/`) - SDL3 Grid Display

* **Grid layout:** Fixed 80×39 grid with Leggie 9×18 font and 9px top padding.
* **Performance:** Dirty flag optimization — only redraws when screen content or cursor changes.
* **Font support:** Printable ASCII (32–126) with run-length encoded glyph rendering.  
  Other codepoints are rendered as space.
* **Color support:** Full 24-bit RGB foreground and background colors.
* **Cursor:** Underline style, 500ms blink cycle with proper visibility control.
* **Scrolling:** Efficient memmove-based scroll operations for terminal output.
* **Architecture:** Separation of screen buffer and rendering for optimal performance.

### 5.3 Terminal Emulator (`term/`) - libvterm Integration

* **Emulation:** Complete VT100/xterm sequence support with libvterm-0.3.3.
* **Text support:** Full UTF-8 input and display with proper multi-byte character handling (not suported by renderer yet).
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

### 5.4 SSH Engine (`ssh_manager/` + `ssh_client/`) - Connection Management

* **Architecture:** Two-layer design separating business logic from protocol implementation
* **SSH Manager:** High-level connection state management, user interaction flow, error handling
* **SSH Client:** Low-level libssh2 wrapper handling protocol details, authentication, data transfer
* **Authentication:** Password-only authentication with interactive prompting
* **Connection flow:** Multi-step user input (hostname → username → port → password) with field validation
* **Crypto:** Standard SSH algorithms via libssh2 + mbedTLS
* **Host key:** Basic host key verification (not implemented yet)
* **Environment setup:** Sets TERM=xterm-256color, COLORTERM=truecolor, COLUMNS=80, LINES=39
* **Error handling:** Comprehensive error reporting and connection retry logic
* **State management:** Non-blocking I/O with proper SSH connection state tracking

### 5.5 Input System (`input_system/`) - Multi-Mode Input Handling

* **Architecture:** Mode-based input routing system supporting different application states
* **Input modes:** 
  * Startup menu choice (test/ssh)
  * SSH connection setup (hostname/username/port/password fields)
  * Normal terminal operation
  * Disconnect/retry prompts
* **Field management:** Generic input field abstraction with validation, masking, and defaults
* **Key processing:** 
  * Printable characters via SDL_EVENT_TEXT_INPUT for proper UTF-8
  * Control and navigation keys mapped to appropriate escape sequences
  * Mode-specific key handling (Enter for field progression vs. terminal input)

### 5.6 UI Manager (`ui_manager/`) - User Interface Layer

* **Responsibility:** All user interface presentation logic separated from business logic
* **Features:**
  * Startup menu display with mode selection
  * SSH connection setup with field-by-field prompts
  * Connection status messages (connecting, success, error)
  * Input validation feedback
  * Screen clearing and header formatting
* **Architecture:** Pure presentation layer - receives formatted data and displays it via terminal
* **Integration:** Works through terminal interface for consistent text-based UI

### 5.7 Keyboard Input (`keyboard/`) - SDL3 Event Processing

* **Architecture:** Event-driven input processing with generic modifier key system
* **Key categories:**
  * Printable characters handled via SDL_EVENT_TEXT_INPUT for proper UTF-8
  * Control and navigation keys mapped to VT100/xterm escape sequences
  * Modifier combinations (Ctrl/Alt/Shift) processed with precedence rules
* **Special handling:**
  * Alt + letter combinations have precedence over Ctrl for letters
  * All navigation keys support modifier combinations for advanced terminal navigation
  * Ctrl+Q reserved for application exit (not sent to terminal)

### 5.8 Test Mode (`test_mode/`) - Terminal Feature Testing

* **Purpose:** Standalone terminal testing without requiring SSH connection
* **Features:** Color display tests, character rendering validation, input echo testing
* **Architecture:** Independent component for validating terminal functionality
* **Usage:** Accessible via startup menu for development and debugging

---

## 6. Storage & Configuration

* **Current implementation:** No persistent storage implemented yet
* **Planned storage:** `known_hosts` file in OpenSSH format for host key verification
* **Configuration:** All connection parameters entered interactively at runtime
* **No persistent settings:** No Connection history, saved credentials, or configuration files yet
* **Storage backend:** Will use BadgeVMS flash filesystem when implemented

---

## 7. Component Interfaces & Data Flow

The application uses a clean component-based architecture with well-defined interfaces:

### 7.1 Core Data Structures

* **`app_state_t`:** Central application state containing SSH connection status, input mode, and connection parameters
* **`connection_input_t`:** User-entered SSH connection parameters (hostname, username, port, password)
* **`input_field_t`:** Generic input field abstraction for unified field handling
* **`ssh_client_t`:** SSH connection state and libssh2 handles

### 7.2 Interface Contracts

* **Application Controller ↔ All Components:** Coordinates initialization, main loop, and shutdown
* **SSH Manager ↔ SSH Client:** High-level connection management using low-level protocol operations
* **Input System ↔ UI Manager:** Input mode routing and field management with presentation separation
* **Terminal ↔ Renderer:** VT100 emulation data flows to optimized grid display
* **All Components ↔ App State:** Shared state access with clear ownership boundaries

### 7.3 Data Flow Patterns

1. **User Input Flow:** SDL events → Keyboard → Input System → SSH Manager/Terminal
2. **SSH Data Flow:** SSH Client ← → libssh2 ← → Network, SSH Client → Terminal → Renderer
3. **UI Flow:** SSH Manager/Input System → UI Manager → Terminal → Renderer
4. **State Flow:** All components read/write app_state with controller coordination

All interfaces are documented in their respective header files (`*.h`) with comprehensive parameter documentation.

---

## 8. Security Implementation

* **Host Key Verification:** Basic SSH host key checking + TOFU storage (planned)
* **Authentication:** Password-only authentication with secure memory handling
* **Crypto:** Standard SSH algorithms via libssh2 + mbedTLS:
  * Key exchange: curve25519-sha256, ecdh-sha2-nistp256
  * Ciphers: chacha20-poly1305@openssh.com, aes128-gcm, aes256-gcm
  * MACs: Built into AEAD ciphers
* **No agent forwarding, port forwarding, or X11 forwarding**
* **Connection security:** Proper SSL/TLS verification through mbedTLS
* **Memory management:** Secure cleanup of password and sensitive data

---

## 9. Build Structure & Dependencies

```
/sdk_apps/sshterm/
  /components/                    # Application components
    /app_controller/              # Main application lifecycle
    /input_system/                # Input mode routing
    /ssh_manager/                 # SSH business logic  
    /ssh_client/                  # SSH protocol wrapper
    /ui_manager/                  # User interface layer
    /renderer/                    # SDL3 display rendering
    /term/                        # Terminal emulation
    /keyboard/                    # Input event processing
    /test_mode/                   # Terminal testing
  /common/                        # Shared data structures
    types.h                       # Common type definitions
    app_state.h                   # Application state structure
  /thirdparty/                    # External dependencies
    /libvterm-0.3.3/              # Terminal emulation library
    /libssh2-1.11.1/              # SSH protocol library
    /mbedtls-3.6.4/               # Crypto and TLS library
  main.c                          # Application entry point
  CMakeLists.txt                  # Build configuration
  ARCHITECTURE.md                 # This document
```

**Dependencies:**
* **libvterm-0.3.3** (MIT)
* **libssh2-1.11.1** (BSD-2-Clause)
* **mbedTLS-3.6.4** (Apache-2.0)
* **SDL3** (zlib) - via BadgeVMS abstraction

---

## 10. Error Handling & Recovery

* **Connection Errors:** Comprehensive error reporting with specific failure reasons
* **Network Issues:** Graceful handling of connection drops with user-friendly error messages
* **Authentication Failures:** Clear feedback on authentication problems with retry options
* **Recovery Strategy:** After any connection failure, return to prompt allowing Retry with same parameters
* **Input Validation:** Field-level validation with immediate feedback for invalid entries
* **Resource Management:** Proper cleanup of SSH sessions, sockets, and memory on all error paths
* **UI Consistency:** All errors presented through terminal interface with color coding

---

## 11. Current Implementation Status

### ✅ **Fully Implemented and Working**

* **Complete VT100/xterm terminal emulation** with libvterm-0.3.3
* **Full SSH connectivity** with password authentication via libssh2
* **Optimized rendering system** with dirty-flag optimization and 80×39 grid
* **Interactive SSH setup** with field-by-field input validation
* **Multi-mode input system** supporting startup menu, SSH setup, and terminal operation
* **Terminal test mode** for feature validation without SSH connection
* **Component-based architecture** with clean separation of concerns
* **Comprehensive error handling** and connection retry logic
* **24-bit RGB color support** with bold text rendering (as brighter text)
* **Complete keyboard mapping** including arrows, modifiers, and special keys
* **Non-blocking SSH I/O** with proper state management

### 🔄 **Planned Improvements**

* **WiFi + Keepalive:** Keep alive over Wi-Fi with simple reconnect logic.
* **Host key verification:** Basic host key verification
* **Persistent known_hosts storage** for TOFU host key verification
* **Connection keep-alive** and automatic reconnection on network drops
* **Enhanced input validation** with more specific error messages
* **Soft-key mapping interface** for keys not physically present on badge

### ❌ **Explicitly Not Planned (yet)**

* Public-key authentication (would require key management UI)
* Multiple simultaneous sessions or tabs
* SFTP file transfer capabilities
* Mouse support or advanced Unicode fonts
* OSC52 clipboard integration
* Scrollback history viewer
* Performance optimizations for high-throughput SSH sessions


---

## 12. Performance Characteristics

* **Rendering Performance:** Dirty-flag optimization ensures only changed areas are redrawn
* **Memory Usage:** Fixed 80×39 character grid (~12KB for screen buffer)
* **Network Performance:** Non-blocking I/O prevents UI freezing during SSH operations
* **Input Latency:** Direct SDL event processing with minimal buffering
* **SSH Throughput:** Capable of handling full terminal bandwidth with real-time display updates
* **CPU Usage:** Single-threaded design with efficient event-driven architecture

---

## 13. Licensing

This application is licensed under GPLv3. See `LICENSE` in this directory for the summary and the repository root `COPYING` for the full license text. Third-party components and required attributions (including the Leggie font, libvterm, and SDL3) are listed in `THIRD_PARTY_NOTICES.md`.