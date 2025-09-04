---
applyTo: "sdk_apps/sshterm/**"
---

# SSH Terminal for WHY2025 Badge - Development Context

## Repository Structure
This repository contains the complete firmware and operating system code for the WHY2025 badge incl. the SDK for applications. The OS is called **BadgeVMS**.

The SSH Terminal app is one of several applications built on top of BadgeVMS.
All other applications can be found in the `sdk_apps/` directory. It might be helpful to explore other apps to understand common patterns and practices or find examples of using platform APIs.

Also examining the BadgeVMS core code in `badgevms/` can provide insights into system-level APIs and services available to apps. In `sdk_include` you can find shared headers and definitions used across the entire codebase.

### Development Scope
We are specifically developing the **sshterm** app within this ecosystem.
The app is located at `sdk_apps/sshterm/`. It is designed to provide SSH terminal functionality on the badge hardware.

**Important**: Changes to BadgeVMS core are possible but must be done in the upstream repository. If BadgeVMS modifications are needed, we must submit merge requests to the upstream repo rather than making local changes.

## Architecture Documentation
We maintain comprehensive architecture documentation in `ARCHITECTURE.md`. **Always consult this documentation** to fully understand the app architecture before implementing new features. Keep the documentation up-to-date when architectural changes are made.

### Architecture Overview
This is a brief overview of the architecture. For full details, see `ARCHITECTURE.md`.

The SSH Terminal app follows a modular component-based architecture:

```
sdk_apps/sshterm/
├── badgevms_stubs/                # BadgeVMS API stubs for local development
├── common/                        # Shared application data structures (app_state.h)
├── components/                    # Application components (core business logic)
│   ├── app_controller/            # Main application lifecycle and event loop coordination
│   ├── common/                    # Shared configuration (terminal_config.h) and data structures
│   ├── input_system/              # Multi-mode input routing and field management
│   ├── keyboard/                  # SDL3 keyboard event processing
│   ├── renderer/                  # Low-level SDL3 graphics rendering (80×39 grid, font data)
│   ├── ssh_client/                # Low-level SSH protocol wrapper (wolfSSH + custom crypto port)
│   ├── ssh_manager/               # High-level SSH connection lifecycle management
│   ├── term/                      # VT100/xterm terminal emulation (libvterm integration)
│   ├── test_mode/                 # Terminal feature testing without SSH connection
│   └── ui_manager/                # Pure presentation layer for user interface
├── sys/                           # System integration (System API wrappers)
├── thirdparty/                    # External dependencies (libvterm, wolfSSH, wolfSSL)
├── main.c                         # Application entry point and BadgeVMS integration
├── CMakeLists.txt                 # Local development build configuration
├── ARCHITECTURE.md                # Comprehensive architectural documentation
└── README.md                      # Basic app overview and build instructions

```

**Target Platform**: ESP32-P4 WHY2025 conference badge with 720x720 display, ESP32-C6 for WiFi/BLE

**Core Architecture**: Component-based design with clear separation of concerns:
- **Threading Model**: Hybrid approach - main SDL event loop + dedicated SSH I/O worker threads
- **Display**: Fixed 80×39 terminal grid using Leggie 9×18 font with dirty-flag optimization
- **SSH Implementation**: wolfSSH/wolfSSL with blocking I/O + threading for BadgeVMS compatibility
- **Input Processing**: Mode-based routing system (startup/SSH setup/terminal operation)

**Key Technical Decisions**:
- Blocking I/O with threading instead of async I/O for BadgeVMS compatibility. Async I/O was not working reliably on BadgeVMS.
- libvterm for VT100 emulation with custom renderer for performance
- Component isolation for maintainability and testing
- Event-driven coordination between blocking SSH operations and UI

**External Dependencies**: The `thirdparty/` directory contains vendored libraries with full source code, including libvterm, wolfSSH, and wolfSSL. These libraries may include examples, test code, and documentation that can serve as reference material for understanding proper API usage and integration patterns.

## Key Principles
- **Separation of Concerns**: Each component has a clear responsibility that shall not overlap with others. This separation shall be preserved at all times. If new functionality is built that does not fit an existing component, a new component shall be created.
- **Modularity**: Components interact through well-defined interfaces. Internal implementation details are encapsulated
- **BadgeVMS Integration**: Components use BadgeVMS APIs for hardware access (crypto, display, WiFi, etc.) rather than direct ESP-IDF calls.
- **Dual Build Support**: Code must work in both local development and hardware environments
- **Error Handling**: Robust error handling and user feedback are essential, especially for network operations.

## Build System
There are **two separate build configurations** for this app:

### 1. Local Development Build
For testing on macOS/Linux hosts during development using the CMakeLists.txt inside `sdk_apps/sshterm/`.

**Build Commands**:
```bash
cd <repo_basedir>/sdk_apps/sshterm
./build.sh
```

**Characteristics**:
- Uses SDL3 for graphics/input simulation
- Hardware functions (crypto, WiFi) are stubbed or simulated
- CMake build system
- Conditional compilation: `SSHTERM_LOCAL_BUILD` defined

### 2. Production Hardware Build
Production build for the physical badge hardware, integrated with the BadgeVMS build system.

**Build Commands**:
```bash
export IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.9_env
export IDF_TARGET=esp32p4
source ~/esp/v5.5/esp-idf/export.sh
cd <repo_basedir>
idf.py build flash monitor # flash and monitor are optional
```

**Characteristics**:
- BadgeVMS SDL3 integration
- Might use hardware functions via BadgeVMS APIs
- ESP-IDF build system (also cmake based)
- No `SSHTERM_LOCAL_BUILD` defined

### Build System Requirements
When modifying build configuration, **ensure changes are reflected in both environments**:
- Update both relevant `CMakeLists.txt` files - sdk_apps/sshterm/CMakeLists.txt (local) and sdk_apps/CMakeLists.txt (hardware)

## Code Quality Standards
- Follow architectural separation of concerns
- Maintain dual build compatibility
- Add comprehensive error handling
- Update ARCHITECTURE.md for significant changes
- Consider BadgeVMS resource constraints
- Use BadgeVMS APIs for hardware access, never direct ESP-IDF calls
- Write clear, maintainable code with appropriate commenting
- Always ensure well-defined interfaces between components and up-to-date headers

## Key Data Structures & Interfaces

### Core Application State
- **`app_state_t`**: Central application state in `common/app_state.h`
  - Current UI state, SSH connection info, terminal state, input mode, etc.
  - Passed to all components for shared context

### Component Interfaces

- Each component has a public interface defined in its header file (e.g., `ssh_manager.h`, `ui_manager.h`)

## Development Workflow

### Testing & Debugging
- **Local Development**: Use `./build.sh` for local rapid iteration with SDL3 simulation
- **Hardware Testing**: Use `idf.py build flash monitor` for actual badge deployment
- **Component Testing**: `test_mode` component allows terminal testing without SSH connection
- **Debugging**: Local builds support standard GDB; hardware uses `idf.py monitor` for serial output

### Common Development Tasks
- **Adding UI Screens**: Implement in `ui_manager` component, coordinate via `app_controller`
- **SSH Protocol Changes**: Modify `ssh_client` low-level wrapper, update `ssh_manager` coordination
- **Input Handling**: Update `input_system` for new modes, `keyboard` for key mappings
- **Display Changes**: Modify `renderer` for graphics, `term` for terminal emulation

### Critical Guidelines

#### ⚠️ MANDATORY: Architecture-First Development
**Before implementing ANY new features or fixing bugs:**

1. **Read ARCHITECTURE.md completely** - Understand the full system design
2. **Identify the correct component(s) and respect boundaries** - Keep the separation of concerns and don't cross component boundaries
3. **Understand existing patterns** - Extend proven approaches, don't replace them
4. **Respect the threading model** - Never bypass the SSH worker thread system
5. **Plan first, then execute** - No not start implementing anything before having agreed on a solid way forward
6. **Test incrementally** - Verify each change doesn't break existing functionality before proceeding

### Implementation Status
- **✅ Working**:
  - Complete VT100 emulation
  - SSH password auth
  - Basic rendering
  - Basic UI navigation
  - Connection management
  - Local development build
  - Hardware build and deployment
  - BadgeVMS integration
  - Basic error handling and user feedback
- **🔄 In Progress**:
  - SSH keyboard-interactive auth
  - Improve UI/UX, e.g. better ordering of input fields
  - Use Hardware RNG (depends on BadgeVMS update)
- **❌ Known Issues and Limitations**:
  - Rendering issues (e.g. with htop, cmatrix)
  - UI/UX issues (to be tested and improved)
  - No host key verification and TOFU
  - No public key auth
  - Limited charset (only basic ASCII)
  - Limited keyboard mapping (no F-keys, limited special chars)
  - No hardware crypto offloading
