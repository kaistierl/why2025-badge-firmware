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
* SSH connections to remote hosts with password authentication via wolfSSH
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
│  │ SSH Manager  │    │Input System │    │ UI Manager   │    │ Test Mode  │  │
│  │ (Connection, │◄──►│(Input mode  │◄──►│ (UI displays │    │ (Terminal  │  │
│  │  threading,  │    │ routing)    │    │  & prompts)  │    │  testing)  │  │
│  │  UI flows)   │    │             │    │              │    │            │  │
│  └──────┬───────┘    └─────┬───────┘    └──────┬───────┘    └────────────┘  │
│         │                  │                   │                            │
│         │                  │                   │                            │
│  ┌──────▼───────┐          │            ┌──────▼──────┐                     │
│  │  SSH Client  │          │            │  Terminal   │                     │
│  │ (wolfSSH +   │          │            │ (libvterm)  │                     │
│  │  wolfSSL)    │          │            │             │                     │
│  │              │          │            └──────┬──────┘                     │
│  └──────┬───────┘          │                   │ screen updates             │
│         │ blocking I/O     │                   ▼                            │
│         ▼                  │            ┌─────────────┐                     │
│   TCP sockets              │            │  Renderer   │                     │
│   (lwIP/POSIX)             │            │ (SDL3 grid) │─► Display           │
│                            │            │             │   Framebuffer       │
│                            │            └─────────────┘                     │
│                            │                                                │
│    ┌─────────────┐         │                                                │
│    │  Keyboard   │─────────┘                                                │
│    │ (SDL events)│                                                          │
│    └─────────────┘                                                          │
│                                                                             │
│ App State: SSH connection status, input mode, connection parameters         │
└─────────────────────────────────────────────────────────────────────────────┘

Data Flow:
• Keyboard → Input System → SSH Manager/Terminal
• SSH Manager ← → SSH Client ← → Network (blocking I/O + threading)
• SSH Client → Terminal → Renderer → Display
• UI Manager → Terminal → Renderer (for menus/prompts)
```

**Concurrency model:** **Hybrid threading with blocking I/O:** Main UI thread + SSH thread + dedicated I/O worker threads using traditional blocking sockets. Initially a Single-threaded event loop (SDL events + non-blocking SSH I/O) was planned, but this never worked well with BadgeVMS.

**Component Architecture:** Clean separation with well-defined interfaces:
- **App Controller:** Main application lifecycle and component coordination
- **SSH Manager:** High-level SSH connection management with threading and UI flow logic
- **SSH Client:** Low-level SSH protocol implementation (wolfSSH wrapper)
- **Input System:** Input mode routing and field management for different app states
- **UI Manager:** User interface rendering and prompt management
- **Terminal/Renderer:** VT100 emulation and optimized display rendering
- **Test Mode:** Standalone terminal testing without SSH connection

---

## 3. Dependencies (GPLv3-compatible)

* **Terminal:** `libvterm-0.3.3` (MIT)
* **SSH:** `wolfSSH-1.4.20` (GPLv3)
* **TLS/Crypto:** `wolfSSL-5.8.2` (GPLv3)
* **Rendering:** `SDL3` (zlib) via BadgeVMS
* **Network:** ESP-IDF sockets (lwIP)

---

## 4. Display & Grid

* **Default font:** Leggie **9×18** bitmap.
* **Default grid:** **80×39** (720/9 = 80 cols; 702/18 = 39 rows; padding top/bottom).
* **Configuration:** Terminal dimensions centralized in `components/common/terminal_config.h`
* **Constants:** `TERMINAL_COLS=80`, `TERMINAL_ROWS=39` used throughout codebase
* `TERM=xterm-256color`, `COLORTERM=truecolor`.

---

## 5. Modules

### 5.1 Application Controller (`app_controller/`)

* **Responsibility:** Main application lifecycle, event loop coordination, and component integration
* **Architecture:** Single-threaded SDL event loop that coordinates with threaded SSH operations
* **Key features:**
  * SDL window and renderer initialization
  * Event routing between input system, SSH manager, and UI manager  
  * Application state management and graceful shutdown handling
  * Polling of threaded SSH operations for data and state updates
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

* **Architecture:** Multi-layer design with **blocking I/O + threading** approach for BadgeVMS compatibility
* **SSH Manager:** High-level connection state management, UI flow logic, and threaded operation coordination
* **SSH Client:** Low-level wolfSSH wrapper with custom blocking I/O callbacks for socket operations
* **Configuration:** Centralized SSH configuration constants in `ssh_config.h`

**Socket & I/O Architecture:**
* **Socket mode:** Traditional blocking TCP sockets (no select/poll for BadgeVMS compatibility)
* **Threading strategy:** Main SSH thread + separate input/output worker threads for concurrent I/O
* **Custom I/O:** wolfSSH configured with `WOLFSSH_USER_IO` using blocking read/write system calls
* **I/O callbacks:** `custom_io.c` provides `wolfssh_io_recv()`/`wolfssh_io_send()` with direct read/write
* **Error handling:** Socket errors and connection drops handled through blocking I/O return codes

**wolfSSL/wolfSSH Integration:**
* **Build config:** Custom `user_settings.h` optimized for SSH-only, single-threaded BadgeVMS environment
* **Features disabled:** TLS server, session cache, multithreading, filesystem dependencies
* **Crypto setup:** AES, RSA, ECC, DH, SHA256, HMAC with FFDHE groups for key exchange
* **Memory model:** `WOLFSSL_SMALL_STACK` and embedded optimizations for resource-constrained environment
* **Random generation:** Custom RNG implementation in `badge_crypto_port.c` for entropy (TODO: Improve this, check what ESP32 hardware can do)

**Authentication & Connection:**
* **Auth method:** Password-only authentication with interactive prompting
* **Connection flow:** Multi-step user input (hostname → username → port → password) with field validation
* **Host key verification:** Basic SSH host key checking (TOFU support planned)
* **Terminal setup:** Sets terminal size to 80×39 characters via SSH protocol
* **State management:** Event-driven coordination between blocking I/O threads and main UI thread

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

* **Responsibility:** Pure presentation layer for user interface rendering separated from business logic
* **Features:**
  * Startup menu display with mode selection
  * SSH connection setup with field-by-field prompts
  * Connection status messages (connecting, success, error)
  * Input validation feedback
  * Screen clearing and header formatting
* **Architecture:** Presentation-only component that displays formatted data via terminal interface
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
* **`ssh_client_t`:** SSH connection state and wolfSSH handles

### 7.2 Interface Contracts

* **Application Controller ↔ All Components:** Coordinates initialization, main loop, and shutdown
* **SSH Manager ↔ SSH Client:** Layered SSH connection management with threading abstraction
* **SSH Manager ↔ UI Manager:** SSH connection flow coordination with presentation services
* **Input System ↔ SSH Manager:** Input mode routing and SSH field management
* **Input System ↔ UI Manager:** General input routing with presentation separation
* **Terminal ↔ Renderer:** VT100 emulation data flows to optimized grid display
* **All Components ↔ App State:** Shared state access with clear ownership boundaries

### 7.3 Data Flow Patterns

1. **User Input Flow:** SDL events → Keyboard → Input System → SSH Manager/Terminal
2. **SSH Connection Flow:** SSH Manager → SSH Client (blocking connection setup via internal threads)
3. **SSH Data I/O Flow:** 
   * **Outbound:** Input System → SSH Manager → SSH Client (blocking send via internal threads)
   * **Inbound:** SSH Client (blocking recv via internal threads) → SSH Manager → Terminal
4. **SSH Terminal Flow:** SSH Client → Terminal → Renderer (for SSH session data display)
5. **UI Flow:** SSH Manager → UI Manager → Terminal → Renderer (for prompts and menus)
6. **Thread Communication:** Event/command queues between main thread and SSH worker threads
7. **State Flow:** All components coordinate via app_state with thread-safe event communication

All interfaces are documented in their respective header files (`*.h`) with comprehensive parameter documentation.

---

## 8. Security Implementation

* **Host Key Verification:** SSH host key checking with TOFU (Trust On First Use) storage (planned)
* **Authentication:** Password-only authentication with secure memory handling
* **Crypto Configuration:** wolfSSL/wolfSSH optimized for embedded SSH-only environment:
  * **Key exchange:** FFDHE groups (2048, 3072, 4096-bit) for Diffie-Hellman key exchange
  * **Ciphers:** AES-CBC, AES-CTR, ChaCha20-Poly1305 (configurable via wolfSSH cipher suites)
  * **Hash algorithms:** SHA-256, HMAC with built-in AEAD cipher support
  * **Public key types:** RSA, ECDSA, Ed25519 support for host verification
  * **Elliptic curves:** ECC support for efficient key operations
* **Build Security:** SSH-only build with TLS/server features disabled (`NO_TLS`, `NO_WOLFSSL_SERVER`)
* **Memory Security:** Small stack configuration (`WOLFSSL_SMALL_STACK`) with secure credential cleanup
* **Random Generation:** Custom entropy source via `badge_generate_seed()` in `badge_crypto_port.c`
* **Threading Security:** Single-threaded wolfSSL configuration (`SINGLE_THREADED`, `NO_WOLFSSL_MULTITHREADING`)
* **Library Validation:** wolfSSL FIPS 140-2 Level 1 validated cryptographic module
* **No agent forwarding, port forwarding, or X11 forwarding**

---

## 9. Build Structure & Dependencies

```
/sdk_apps/sshterm/
  /components/                    # Application components
    /app_controller/              # Main application lifecycle
    /input_system/                # Input mode routing
    /ssh_manager/                 # SSH business logic and components
      ssh_manager.c/.h            # High-level SSH connection management  
      ssh_ui_controller.c/.h      # SSH UI flow logic and state management
      ssh_thread.c/.h             # Threaded SSH operations
      ssh_config.h                # Centralized SSH configuration constants
    /ssh_client/                  # SSH protocol wrapper
      ssh_client.c/.h             # wolfSSH wrapper with blocking I/O
      custom_io.c/.h              # Custom blocking I/O callbacks for wolfSSH
      user_settings.h             # wolfSSL/wolfSSH build configuration
      badge_crypto_port.c         # Custom RNG implementation for BadgeVMS
    /ui_manager/                  # User interface layer
    /renderer/                    # SDL3 display rendering
    /term/                        # Terminal emulation
    /keyboard/                    # Input event processing
    /test_mode/                   # Terminal testing
    /common/                      # Shared configuration
      terminal_config.h           # Terminal dimension constants (80×39)
  /common/                        # Shared data structures
    types.h                       # Common type definitions
    app_state.h                   # Application state structure
  /thirdparty/                    # External dependencies
    /libvterm-0.3.3/              # Terminal emulation library
    /wolfssh-1.4.20/              # SSH protocol library
    /wolfssl-5.8.2/               # Crypto and SSL/TLS library
  main.c                          # Application entry point
  CMakeLists.txt                  # Build configuration
  ARCHITECTURE.md                 # This document
```

**Dependencies:**
* **libvterm-0.3.3** (MIT)
* **wolfSSH-1.4.20** (GPLv3)
* **wolfSSL-5.8.2** (GPLv3)
* **SDL3** (zlib) - via BadgeVMS abstraction

---

## 10. Error Handling & Recovery

* **Connection Errors:** Comprehensive error reporting through threaded event system with specific failure reasons
* **Network Issues:** Graceful handling of blocking socket errors and connection drops via thread communication
* **Threading Errors:** Robust thread lifecycle management with proper startup/shutdown coordination
* **Authentication Failures:** Clear feedback on authentication problems through SSH event system with retry options
* **Recovery Strategy:** After any connection failure, return to prompt allowing retry with same parameters
* **Input Validation:** Field-level validation with immediate feedback for invalid entries
* **Resource Management:** Proper cleanup of SSH sessions, threads, sockets, and wolfSSL/wolfSSH resources on all error paths
* **UI Consistency:** All errors presented through terminal interface with thread-safe event delivery

---

## 11. Current Implementation Status

### ✅ **Fully Implemented and Working**

* **Complete VT100/xterm terminal emulation** with libvterm-0.3.3
* **Full SSH connectivity** with password authentication via wolfSSH using blocking I/O + threading
* **Optimized rendering system** with dirty-flag optimization and 80×39 grid
* **Interactive SSH setup** with field-by-field input validation
* **Multi-mode input system** supporting startup menu, SSH setup, and terminal operation
* **Terminal test mode** for feature validation without SSH connection
* **Component-based architecture** with clean separation and threaded SSH operations
* **Comprehensive error handling** with thread-safe event communication and connection retry logic
* **24-bit RGB color support** with bold text rendering (as brighter text)
* **Complete keyboard mapping** including arrows, modifiers, and special keys
* **Blocking I/O SSH architecture** with dedicated worker threads for non-blocking UI

### 🔄 **Planned Improvements**

* **WiFi + Keepalive:** Keep alive over Wi-Fi with simple reconnect logic
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
* **Network Architecture:** **Blocking I/O with threading** prevents UI freezing while maintaining simple socket operations
* **Threading Overhead:** Lightweight worker threads for I/O operations with efficient queue-based communication
* **Input Latency:** Direct SDL event processing with minimal buffering and thread-safe command queuing
* **SSH Throughput:** Full terminal bandwidth support through dedicated I/O threads with blocking socket operations
* **CPU Usage:** Multi-threaded design with main UI thread + SSH coordination thread + dedicated I/O workers
* **wolfSSL/wolfSSH Performance:** Small-stack embedded configuration optimized for resource-constrained environment

---

## 13. Licensing

This application is licensed under GPLv3. See `LICENSE` in this directory for the summary and the repository root `COPYING` for the full license text. Third-party components and required attributions (including the Leggie font, libvterm, wolfSSH, wolfSSL, and SDL3) are listed in `THIRD_PARTY_NOTICES.md`.