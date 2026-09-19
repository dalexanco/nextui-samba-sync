# Technical architecture

This document details the pak's technical implementation. For the functional
description (screens, flows), see [SPEC.md](../SPEC.md). For a quick overview,
see [CLAUDE.md](../CLAUDE.md).

## Source tree

```
nextui-samba-sync/
  pak.json
  launch.sh
  build-desktop.sh / build-tg5040.sh / build-tg5050.sh / run-docker.sh
  .nextui-workspace          (symlink)
  src/
    Makefile
    main.c
    screens/
      links_list.c/.h        (screen 1)
      link_detail.c/.h       (screen 2)
    sync_config.c/.h         (reads Samba Sync.toml)
    link_state.c/.h          (persisted last sync)
    sync_queue.c/.h          (check / sync queue)
    sync_engine.c/.h         (one link's check and sync)
    smb_client.c/.h          (libsmb2 wrapper)
    local_fs.c/.h            (listing and creation on the SD side)
    browse.c/.h              (BrowseFileEntry + path helpers)
    ui.c/.h                  (shared rendering helpers)
    vendor/tomlc17/          (vendored TOML parser, see VENDORED.md)
  lib/
    libsmb2/                 (git submodule)
    build-libsmb2.sh
```

The config module is named `sync_config` rather than `config`: `-I.` puts
`src/` ahead of `all/common/`, so a local `config.h` would shadow NextUI's own,
which `api.c` includes.

## Data model and persistence

### Configuration (read-only as far as the pak is concerned)

`SDCARD_PATH "/Samba Sync.toml"`, read by `sync_config_load()`. Three tables:
`[settings]`, `[servers."<name>"]`, `[links."<name>"]` — keys and values
detailed in [SPEC.md](../SPEC.md).

In memory: two fixed arrays (`MAX_SERVERS`, `MAX_LINKS`), no dynamic
allocation. An invalid link is **kept**, with its `config_error` filled in
(message shown as-is), so screen 1 can show it as failed instead of letting it
vanish silently. A missing file or malformed TOML yields zero links and a
`ConfigStatus` that screen 1 turns into an empty state.

Display and execution order is the order of the tables in the file: that is
what tomlc17's `toml_datum_t.u.tab.key[]` gives back, not a guarantee of the
TOML standard (see `src/vendor/tomlc17/VENDORED.md`).

### Persistent state

`$SHARED_USERDATA_PATH/samba-sync/state/<slug>.txt`, one `key=value` file per
link, written by `link_state.c`:

```
name=Roms GBA
status=partial
time=1758318000
copied=84
bytes=1288490188
deleted=5
errors=2
error=Not enough space	Pokemon Ruby (USA).gba
error=Write refused	Zelda Minish Cap (EU).gba
```

The `name` is read back to ignore a file belonging to another link whose name
slugifies the same way. `error=` lines carry the reason then the path,
separated by a tab (reasons never contain one, paths may contain anything
else); at most 50 are kept, `errors=` gives the real total.

## Screen state machine

Two screens (`enum Screen` in `main.c`), each exposing `input(&dirty)` /
`render(screen, show_setting)` — a `switch`, no vtable. The render loop is
"dirty"-gated: only `LinksList_input()` advances the queue, so a check in
progress naturally pauses while screen 2 is open.

## Key modules

### `sync_engine.c/.h`

`sync_engine_check(link)` is **blocking**: connection, recursive remote
listing, recursive local listing, then diff. Both lists are sorted (`qsort` on
`rel_path`) and merged in one pass — v1 compared every remote file to every
local one, up to 16 million `strcmp` at 4096 files per side.

`sync_engine_start(link)` redoes the check (Mirror deletions must rest on an
up-to-date state), then `sync_engine_tick()` advances in slices:

```
COPYING -> [DELETING] -> DONE
        \-> CANCELLED
```

Each tick works for `TICK_BUDGET_MS` (100 ms) rather than for a single chunk:
at 60 fps, one chunk per tick capped the throughput. The chunk itself is
`COPY_CHUNK_SIZE` (1 MB) — libsmb2's synchronous API keeps a single request in
flight, so the chunk size *is* the throughput ceiling (bytes per RTT); libsmb2
trims the request down to the negotiated `max_read_size`. During a sync,
`sync_queue.c` switches the CPU to `CPU_SPEED_PERFORMANCE` and the screen is
redrawn at only 4 Hz: whatever goes into rendering does not go into the
transfer.

Error rules:
- a **per-file** error (open, write, disk space, delete) is recorded and the
  link carries on → `partial` status;
- a **link-wide** error (connection lost, server unreachable) stops the link →
  `error` status;
- each file is written to a hidden `.part` (dot-prefixed, so ignored by
  `hide()` and by NextUI), renamed only once complete and removed on failure or
  cancellation: never a truncated file on the SD card;
- in Mirror mode, deletions only happen if **no** copy failed; folders left
  empty are removed afterwards, never stepping outside the link's local folder;
- a local path that would not fit in `MAX_PATH` is a file error, not a
  truncated path pointing at some other file.

### `sync_queue.c/.h`

Sits above `sync_engine`, walks the valid links in sequence, in check or sync
mode. A failure never stops the queue. Moving a link to `CHECKING`/`SYNCING`
takes a tick of its own, so the screen shows "Checking…" **before** the
blocking call. Keeps, per link, the display phase, the latest `LinkCheck` and
the latest `LinkState`.

### `smb_client.c/.h`

Thin wrapper around libsmb2: `smb_connect` (the share comes from the link),
`smb_list_files_recursive`, `smb_open_read`/`smb_read_chunk`/`smb_close_read`.
Errors are typed from `smb2_get_nterror()` (`LOGON_FAILURE` → auth,
`BAD_NETWORK_NAME` → share, `OBJECT_*_NOT_FOUND` → folder, no NT status at all
→ server unreachable) and `smb_error_label()` gives the label shown.

The remote listing applies the same `hide()` rule as the local one: without it
a remote `.DS_Store` would show up as "new" forever. Going past
`BROWSE_MAX_FILES` is an explicit error: a truncated listing would, in Mirror
mode, delete files that do exist remotely.

### `ui.c/.h`

Shared rendering helpers. `UI_fitText()` truncates on UTF-8 boundaries, unlike
NextUI's `GFX_truncateText()`, which drops 4 bytes at a time and can cut an
accented letter in half.

Text is always laid on a pill (`ASSET_BLACK_PILL`): the background color is a
NextUI theme setting (`COLOR_BACKGROUND`), so text laid straight on the
background has no guaranteed contrast.

Font constraint: `font2.ttf` contains neither `✔` nor `✘`, and neither shipped
font has `⟳`, `🗑` or `⚠`. States are therefore spelled out in words.

## SMB client: why libsmb2

[libsmb2](https://github.com/sahlberg/libsmb2) (sahlberg/libsmb2):
- LGPLv2.1
- ~50KB compiled
- No dependency outside libc (Kerberos explicitly disabled, see below)
- Used in production by RetroArch and Kodi

Rejected alternative: `mount.cifs` / the kernel CIFS module — CIFS support in
the TrimUI's Buildroot kernel is unverified, too risky as a foundation.

Vendored as a pinned git submodule in `lib/libsmb2/`.

## TOML parser: tomlc17

Copied as-is into `src/vendor/tomlc17/` (MIT, one `.c` + one `.h`, no separate
build step), see `VENDORED.md` for the pinned commit. Chosen over tomlc99,
whose README declares the library obsolete.

It uses `static_assert` (C11): the Makefile applies `-std=gnu11` to it through
a dedicated rule, the rest of the pak staying on `-std=gnu99`.

## Build & cross-compilation

- `build-desktop.sh` / `build-tg5040.sh` / `build-tg5050.sh` / `run-docker.sh`
- `.nextui-workspace` symlink
- `ghcr.io/loveretro/${PLATFORM}-toolchain` Docker toolchain
- `CROSS_COMPILE=aarch64-nextui-linux-gnu-`

`lib/build-libsmb2.sh` builds libsmb2 statically per platform with
`-DENABLE_LIBKRB5=OFF -DENABLE_GSSAPI=OFF`: left to auto-detection, Kerberos
stays off in the device toolchains (no krb5 in their sysroot) but turns on in a
native macOS build, which then fails to link against the system GSS.

## Test strategy

- `build-desktop.sh` as the main iteration tool.
- A throwaway Samba Docker container to test real SMB, e.g.:
  `docker run -d -p 1445:445 -v $(pwd)/share:/share dperson/samba -u "tester;secret" -s "Roms;/share;yes;no;no;tester" -p`
- No automated tests in the repo (as in gift-code). The sync modules have no
  SDL dependency, though, so they can be driven from a small command-line
  harness (stubs for `defines.h`/`api.h`/`utils.h`), which covers checking,
  syncing, errors, Mirror and cancellation without any UI.
- Manual validation on device through `build-tg5040.sh`/`build-tg5050.sh` + ADB
  before each release.

## Technical risks / things to confirm

- **Blocking check**: `sync_engine_check()` is synchronous. The UI redraws
  between links but freezes during each one, and B only interrupts between two
  links. To reconsider on a large share; a dedicated thread would be the fix.
- **"Already there" detection by name + size**: a remote file modified without
  a size change is not copied again.
- **File name case**: exFAT/FAT32 is case-insensitive, the comparison done here
  is not — a remote rename limited to case can produce an inconsistent diff.
- **Exact environment variable names** (`$SHARED_USERDATA_PATH`,
  `$SDCARD_PATH`) assumed stable (used as-is by gift-code) but not yet
  confirmed against a real NextUI environment.

## Validation: cross-compiling libsmb2 (2026-09-15)

Tested independently of the app, by pointing `cmake` straight at the
`ghcr.io/loveretro/tg5040-toolchain` container's compiler:

- Toolchain confirmed in the image: `aarch64-nextui-linux-gnu-gcc`
  (crosstool-NG 1.25.0, gcc 8.3.0), `cmake` 3.28.3, `make`, `git`.
- CMake configure with a minimal toolchain file, `-DBUILD_SHARED_LIBS=OFF
  -DENABLE_EXAMPLES=OFF -DENABLE_LIBDCERPC=OFF`: succeeded with no
  intervention.
- `make`: produces `libsmb2.a` (~630KB, `aarch64` ELF).
- Link test with `smb2_init_context()`/`smb2_destroy_context()`: succeeded.

The full pak (libsmb2 + tomlc17 + sources) cross-compiles from there, verified
on `tg5040`.
