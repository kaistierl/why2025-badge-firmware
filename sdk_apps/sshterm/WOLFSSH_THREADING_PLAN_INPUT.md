# wolfSSH Threading — Planning Input

**Purpose:** Technical brief for future agent sessions working on the wolfSSH concurrent
send+receive race in sshterm.

**Status:** Complete. Option A (cipher ring buffer + single wolfSSH caller) implemented.
The concurrent send+receive race is eliminated. See §Option A and §Current Architecture.

---

## Problem Statement

`read_peer_thread` calls `wolfSSH_stream_read` and `read_input_thread` calls
`wolfSSH_stream_send` on the **same `WOLFSSH*` session object** from two threads
simultaneously. wolfSSH has no internal locking. This causes:

- Shared channel state corruption (window sizes, packet sequence numbers, input/output
  buffers)
- Confirmed unhandled exceptions on the ESP32-P4 under rapid key input against a
  high-throughput server (htop + mashing arrow keys)
- Keyboard input lost after crash

**Current state** (as of `96daaa7e`):
- Two-thread model (`read_peer_thread` + `read_input_thread`) with `wolfssh_mutex` around
  sends. Unchanged from the partial fix that was already in place.
- The race window is still present but narrow (microseconds when recv wakes and processes
  a packet while a send is in flight).
- `wolfssh_io_recv_streaming` callback exists as a named hook for a future non-blocking
  implementation — currently identical to `wolfssh_io_recv` (blocking read).
- `ssh_client_receive` WS_WANT_READ detection fixed (see §Fixes landed).

---

## Critical Finding: Two Separate Layers

### Layer 1 — wolfSSL crypto internals (conditionally protected)

`SINGLE_THREADED` and `NO_WOLFSSL_MULTITHREADING` in `user_settings.h` (lines 24–26) disable
all internal locking in wolfSSL's crypto primitives. wolfSSL already ships a complete
FreeRTOS mutex port in `wc_port.c` (lines 1792–1825):

```c
// Activated by: #define FREERTOS  (+ optionally WOLFSSL_ESPIDF)
int wc_InitMutex(wolfSSL_Mutex* m)  { *m = xSemaphoreCreateMutex(); ... }
```

**FreeRTOS headers are NOT available in the SDK include path** (`sdk_include/` contains
only picolibc/newlib/lwip/SDL headers). Option B (enable wolfSSL FreeRTOS mutex port)
is not viable without BadgeVMS exposing FreeRTOS headers.

This layer is moot for Option A (single io thread): with one wolfSSH-calling thread, all
crypto is also single-threaded in practice. `SINGLE_THREADED` can stay as-is.

### Layer 2 — wolfSSH session object (completely unprotected)

wolfSSH (`internal.c`, `ssh.c`) has **zero session-level locking**. wolfSSH was designed
single-threaded. Removing `SINGLE_THREADED` alone protects wolfSSL crypto internals but
does NOT protect the wolfSSH session-level race.

---

## Proposed Solution: Option A — Single wolfSSH I/O thread

Collapse all wolfSSH calls into one thread. The recv side must yield periodically so
the thread can service outgoing sends. The key mechanism for yielding is a non-blocking
or timed recv — which turned out to be the hard part on BadgeVMS.

### Blockers confirmed during implementation (2026-06-16)

**`select()` declared in SDK but does NOT work at runtime for socket fds.**

`sys/_select.h` declares `select()`, `fd_set`, `FD_SET`, `FD_ZERO`. The call compiles
and links. However, BadgeVMS maps socket fds through a task-level translation layer
(`why_socket`): the application sees task fd 3, the underlying lwIP socket is device fd 54.
`read()` and `write()` go through this translation, but `select()` bypasses it and calls
lwIP's select with the task fd (3), which is not a recognized lwIP socket. Result:

```
SSH_IO: select error: Success (errno=0)
```

`select()` returns -1 with errno=0 immediately on every call. Cannot be used.

**`fcntl` not in BadgeVMS shared symbol table.** (Confirmed 2026-06-16 by Option C probe.)

ELF load error: `Can't find common fcntl`. `<fcntl.h>` is present in the SDK staging
headers and `F_GETFL`/`F_SETFL`/`O_NONBLOCK` compile fine, but `fcntl` itself is not
exported by the BadgeVMS ELF symbol table. A no-op stub was added to `custom_io.c`
(returns ENOSYS). `O_NONBLOCK` is therefore unavailable on hardware.

**`recv()` not in BadgeVMS shared symbol table.** (Confirmed 2026-06-16 by Option C probe.)

ELF load error: `Can't find common recv`. A stub returning ENOSYS was added to
`custom_io.c`. All socket reads continue to use `read()` which routes through
`why_socket`'s fd translation layer.

**`setsockopt(SO_RCVTIMEO)` not in BadgeVMS shared symbol table.** (Known before this
session — no-op stub already in place in `custom_io.c`.)

**`wolfSSH_connect` cannot be re-entered iteratively across the auth phase.**

A retry loop `while (wolfSSH_connect() != WS_SUCCESS && internal_error == WS_WANT_READ)`
was attempted for the handshake. It fails with:

```
SSH: Connection failed with code -1001 (message not allowed before user authentication)
```

wolfSSH's state machine jumps ahead to session channel open before receiving the server's
auth-success response. The handshake **must use a single blocking `wolfSSH_connect()` call**.
Two separate recv callbacks now exist: `wolfssh_io_recv` (blocking, used during handshake)
and `wolfssh_io_recv_streaming` (installed after `WS_SUCCESS`, currently also blocking).

Additional detail: `wolfSSH_connect()` returns `WS_ERROR` (-1001) on non-success, not
`WS_WANT_READ` (-1010). The internal state is accessible via
`wolfSSH_get_error(ssh) == WS_WANT_READ`.

### Option C probe results (2026-06-16) — all primitives unavailable

| Primitive | Status | Evidence |
|-----------|--------|----------|
| `setsockopt` | Not in symbol table | ELF load failure (pre-existing) |
| `recv` | Not in symbol table | ELF load failure `Can't find common recv` |
| `fcntl` | Not in symbol table | ELF load failure `Can't find common fcntl` |
| `select()` | Links but broken at runtime | Returns -1/errno=0 (bypasses fd translation) |
| `MSG_DONTWAIT` | Not defined in SDK headers | — |
| `O_NONBLOCK` | Symbol unavailable (fcntl stub) | — |

**Conclusion: Option A-lite (single thread + O_NONBLOCK) is not possible.
Full Option A (cipher ring buffer + dedicated `sock_reader_thread`) is the
only viable path that does not require BadgeVMS upstream changes.**

Stubs for all three unavailable symbols are in `custom_io.c` (`#ifndef SSHTERM_LOCAL_BUILD`).

---

## Fixes Landed (commit `96daaa7e`)

### 1. `wolfssh_io_recv_streaming` callback split

`custom_io.c` now has two recv callbacks:
- `wolfssh_io_recv` — blocking `read()`, installed at context creation, used during
  `wolfSSH_connect` (handshake + auth). Must block; see §Blockers above.
- `wolfssh_io_recv_streaming` — installed via `wolfSSH_SetIORecv(ctx, ...)` after
  `wolfSSH_connect` returns `WS_SUCCESS`. Currently delegates to `wolfssh_io_recv`.
  When a non-blocking primitive becomes available, this is the only function to change.

### 2. WS_WANT_READ detection fix in `ssh_client_receive`

The old check used string matching:
```c
if (strstr(wolfSSH_get_error_name(ssh), "WANT_READ") || strstr(..., "AGAIN"))
```
wolfSSH's actual error string for `WS_WANT_READ` is `"I/O callback would read block error"`,
which contains neither substring. The check silently failed and treated WS_WANT_READ as a
fatal error. Fixed to:
```c
if (wolfSSH_get_error((WOLFSSH*)client->ssh) == WS_WANT_READ) return 0;
```

---

## Current Architecture (Option A implemented)

```
ssh_thread_main      ── control commands (CONNECT/DISCONNECT/SHUTDOWN)
sock_reader_thread   ── read(socket_fd) → cipher_rb   [no wolfSSH calls]
ssh_io_thread        ── sole wolfSSH caller:
                           wolfSSH_stream_read (drains cipher_rb, non-blocking)
                           wolfSSH_stream_send (processes SSH_CMD_SEND_RAW_INPUT)
```

- No `wolfssh_mutex` — eliminated because only one thread calls wolfSSH.
- No race window — concurrent access to `WOLFSSH*` is structurally impossible.
- `cipher_rb` (16 KB) in `ssh_client_t` bridges the blocking `read()` in
  `sock_reader_thread` and the non-blocking drain in `wolfssh_io_recv_streaming`.
- `io_work_cond` (on `cmd_queue_mutex`) wakes `ssh_io_thread` when cipher data
  arrives or a send command is queued.

---

## Key Build Facts

- `select()` compiles and links but **returns -1/errno=0 at runtime** on BadgeVMS socket fds
- `<fcntl.h>` is **not in the SDK include path** — O_NONBLOCK cannot be referenced
- `setsockopt` is **not in the BadgeVMS shared symbol table** — no-op stub required
- `MSG_DONTWAIT` is **not defined** in `sdk_include/`
- FreeRTOS headers are **not in the SDK include path** — wolfSSL FreeRTOS mutex port not buildable
- `recv()` is declared in `sdk_include/sys/socket.h` but unknown if in shared symbol table
- `read()` and `write()` are confirmed to work through `why_socket` fd translation
- `wolfSSH_SetIORecv` takes `WOLFSSH_CTX*` (not `WOLFSSH*`) — per-session swap uses the ctx
