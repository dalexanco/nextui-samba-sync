# NextUI Samba Sync — Specification (v2)

A NextUI pak (`TOOL` type) that syncs folders (pull only) from a Samba/SMB share on the local
network onto the console's SD card.

Inspired by the structure and style of [`nextui-gift-code`](https://github.com/dalexanco/nextui-gift-code)
(minimal C pak, no heavy dependencies, state-machine UI built on NextUI's shared widgets).

## What changed since v1

v1 built its jobs on the console (pick a server, browse the remote share, browse the SD card). v2
separates the two roles strictly:

- **Everything describing the sync lives in the config**: servers, remote folders, local
  destinations, mode. One file, edited offline.
- **The UI only triggers and reports**: it shows each link's status, starts the sync and shows the
  result. No creating or editing on the console, no file browser, no settings screen.

## Scope of v2

- **Pull only**: the console downloads from the share onto the SD card. Remote content is never
  modified.
- One or more **links** declared in the config file.
- Each link has a **mode**: *Add* (`add`, default) or *Mirror* (`mirror`).
- Automatic check when the pak opens, manual sync of every link in one press. No background sync.

## Concept

- A **link** = one folder of an SMB share tied to one folder of the SD card (e.g. `Living Room NAS`,
  share `Roms`, folder `GBA` → `Roms/Game Boy Advance (GBA)` on the SD card).
- A **server** = the connection details of a Samba machine (host, port, credentials), declared once
  and referenced by name from the links.
- Remote/local comparison by **name + size**:
  - missing locally → copied;
  - present with a different size → copied again (overwritten);
  - present with the same size → skipped.
- **Add mode**: copies what is new or different, never deletes anything locally.
- **Mirror mode**: additionally deletes, from the local folder, the files and subfolders absent from
  the remote folder. Deletion is strictly limited to the link's local folder. No confirmation
  screen: how many files would be deleted is shown on the link's detail screen after the check,
  before the user starts the sync.
- **No text entry** on the console (unchanged since v1).

## Configuration (file)

A single file at the root of the SD card, `Samba Sync.toml`, edited while the card is mounted on a
computer (or over SSH). Format: [TOML](https://toml.io/en/), parsed by
[tomlc17](https://github.com/cktan/tomlc17) (one `.c` + one `.h`, MIT, vendored in the repo;
tomlc99, its predecessor, is declared obsolete by its author).

```toml
[settings]
timeout = 10

[servers."Living Room NAS"]
host = "192.168.1.10"
port = 445
username = "guest"
password = ""

[links."Roms GBA"]
server = "Living Room NAS"
share = "Roms"
remote = "GBA"
local = "Roms/Game Boy Advance (GBA)"
mode = "mirror"

[links."Roms SNES"]
server = "Living Room NAS"
share = "Roms"
remote = "SNES"
local = "Roms/Super Nintendo Entertainment System (SFC)"
```

**Table `[settings]`** — optional.
- `timeout` — integer, network timeout in seconds, default `10`.

**Tables `[servers."<name>"]`** — one per server; `<name>` identifies it for the links and is shown
on the detail screen.
- `host` — string, required, IP address or hostname.
- `port` — integer, optional, default `445`.
- `username` / `password` — strings, optional, empty or absent = guest access.
- `domain` — string, optional.

**Tables `[links."<name>"]`** — one per link; `<name>` is the name shown on the console and the key
of its persisted state. The order of the tables in the file gives the display and execution order.
- `server` — string, required, name of a `[servers."…"]` table.
- `share` — string, required, SMB share name.
- `remote` — string, optional, path inside the share (empty or absent = share root).
- `local` — string, required, path relative to the SD card root. Created if missing.
- `mode` — string, optional: `"add"` (default, shown as “Add”) or `"mirror"` (shown as “Mirror”).

**Link order**: the TOML standard does not guarantee the order of a table's keys. The pak relies on
tomlc17, which returns them in file order (verified). That is a property of the library, not of the
standard: an external tool that reformats or sorts the file may change the display order. To be
mentioned in the README.

**Error tolerance**:
- Missing file → the main screen shows an empty state explaining where to create
  `Samba Sync.toml`.
- Syntactically invalid file (malformed TOML) → no link can be read: the main screen shows the
  parser's error with its line number ("Samba Sync.toml, line 12: missing quote").
- Malformed server (missing `host`, non-integer `port`) → ignored; the links referencing it show
  the error "Config: invalid server “Living Room NAS”".
- Malformed link (required field missing or of the wrong type, unknown `server`, `local` absolute
  or escaping the SD card via `..`, unknown `mode` value) → **listed in error** with the reason
  ("Config: server “NAS” not found"), without blocking the other links. It is neither checked nor
  synced.
- Two servers or two links with the same name → TOML syntax error (key defined twice): that is the
  "invalid file" case above, no link is loaded.
- Unknown keys ignored, including tables other than `settings`, `servers` and `links` (so options
  can be added later without breaking older versions of the pak).

**Security**: passwords are stored in clear text in the file. To be flagged in the README.

**Persisted state**: each link's last sync result (date, status, counters, and the first 50 errors
along with their total count) is written by the pak to
`$SHARED_USERDATA_PATH/samba-sync/state/<name>.txt`, keyed by link name. Renaming a link in the
file loses its history.

**No symbols**: status labels use neither ✔/✘ nor emoji. The two fonts shipped with NextUI don't
contain them all (`font2.ttf` has neither ✔ nor ✘, neither font has ⟳/🗑/⚠) and a missing glyph
renders as an empty box. Only `·`, `—`, `…`, curly quotes and accented letters are used.

**Language**: keys, table names and enumerated values are English, and so is the on-screen text.

---

## Screens

Two screens only: the list of links, and one link's detail.

### 1. Main screen — list of links

Shown when the pak opens. Checking starts immediately and fills the list progressively, one link
after another.

While checking:

```
Samba Sync
─────────────────────────────────────────
▸ Roms GBA    [Mirror]   [New]
  Roms SNES              Checking…
  Bios                   Waiting

X  Check   Y  Details   B  Quit
```

Check finished:

```
Samba Sync
─────────────────────────────────────────
▸ Roms GBA    [Mirror]   [New]
  Roms SNES              Up to date
  Bios                   Error: Server unreachable
  Saves                  Config: share missing

  Last sync: 18/09 at 18:42 · 1 failed

X  Check   Y  Details   A  Sync   B  Quit
```

**Possible statuses of a link** (right-hand column):
- `Waiting` — not checked yet.
- `Checking…` — connecting and comparing.
- `[New]` — there is something to sync (files to copy and/or, in mirror mode, to delete). **No
  numbers here**: the detail (how many, what volume, how many deletions) is on screen 2, so the
  list stays readable at a glance.
- `Up to date` — nothing to copy or delete.
- `Error: <short reason>` — the check failed (network, authentication, share or folder not found).
  An invalid config shows its reason directly (`Config: …`).
- `Not checked` — checking was interrupted before reaching this link.
- If this link's **last sync** failed (persisted state), a `!` marker is added before its name, even
  when the current check succeeds — the detail is on screen 2.

**Buttons**:
- **A**: Sync everything (see flow below). Inactive until the check has finished, and when no valid
  link exists.
- **X**: re-reads `Samba Sync.toml` (so edits made while the pak is running are picked up) then
  checks every link again.
- **Y**: opens the selected link's detail → screen 2.
- **B**: quits the pak (during a check: interrupts it, then quits).

**Footer**: date of the last sync (across all links) and how many links failed it.

### "Sync everything" flow

The sync runs **on the main screen**, with no dedicated progress screen:

```
Syncing 2/4
─────────────────────────────────────────
  Roms GBA    [Mirror]   12 copied · 3 deleted
▸ Roms SNES              Chrono Trigger.sfc · 62% of 1.4 GB
  Bios                   Waiting
  Saves                  Config: share missing

B  Cancel
```

- Valid links are processed **in sequence**, in file order. Links with a config error are skipped.
  Links that failed their check with a network error are retried (the network may be back).
- For each link: connect → remote listing → comparison with the local side (redone right before
  copying, so mirror deletions rest on an up-to-date diff) → copy file by file → **in mirror mode**,
  deletions last, only if every copy succeeded.
- The active link's line shows the current file and the **progress by volume** (percentage of the
  total to copy, and that total). The number of files says little about the time left when file
  sizes vary; the volume does. During mirror mode's deletion phase the line shows `Deleting n/N`.
- When a link finishes, its line shows its result: `N copied · M deleted`, `Up to date`,
  `Partial · N copied · K errors` if some files failed, or `Error: <reason>`.
- **A failure does not interrupt the queue**: the error is recorded and the next link starts.
- **B**: cancels the current link and every remaining one. What was already copied stays in place;
  mirror deletions already done are not undone, the remaining ones are not performed. The current
  link and the following ones are marked `Cancelled`.
- End of the queue: the title shows "Sync finished", each link's persisted state is updated, and the
  screen's normal buttons return (A syncs again, X re-checks, Y details).

### 2. Link detail screen

Reached with **Y** from the main screen, outside a running sync.

```
Roms GBA
─────────────────────────────────────────
Check       12 new (340 MB)
            3 to delete (1.1 GB)
            -800 MB on the SD card

Last sync   18/09/2026 at 18:42 · partial
            84 copied (1.2 GB)
            5 deleted
            2 errors:
              Pokemon Ruby (USA).gba — Not enough disk space
              Zelda Minish Cap (EU).gba — Write denied

Server      Living Room NAS (192.168.1.10)
Remote      Roms/GBA
Local       Roms/Game Boy Advance (GBA)
Mode        Mirror

B  Back
```

The check and the last sync come first: they are what the screen is opened for, and the
configuration is reference material that would otherwise push the figures off the bottom.

Each line sits on a black pill, like the lines of screen 1: the background colour is a NextUI theme
setting, so text placed directly on it would have no guaranteed contrast.

A value too long for one line **wraps onto several lines** rather than being cut off, its
continuation aligned under the value column — paths and error messages are routinely long.

- **Check**: one figure per line, so each can be read on its own:
  - files to copy and their volume;
  - files to delete and the volume they free (mirror mode only);
  - the **net effect on the SD card**, signed: what arrives, minus the bytes of the local files it
    replaces, minus what is deleted. Negative when mirror mode frees more than it brings in.

  Otherwise: the error encountered, or the config error. After a sync the check is stale: the line
  shows "stale, press X to re-check" rather than a wrong number.
- **Last sync**: date, status (succeeded / partial / failed / cancelled / never), copied and deleted
  counters (one per line), and the list of errors (link-wide error, or per-file errors). The screen
  scrolls (up/down); the list is limited to the first 50 errors, with the total still shown.
- **Configuration**: read-only recap of what the file says.
- **B**: back to the main screen.

---

## Error cases covered

Each appears as a short reason on the main screen, and in full on the detail screen.

| Case                                    | Scope                                              |
|-----------------------------------------|----------------------------------------------------|
| Invalid config (see tolerance)          | Link                                               |
| Host unreachable / timeout              | Link                                               |
| Authentication refused                  | Link                                               |
| Share or remote folder not found        | Link                                               |
| Local folder cannot be created          | Link                                               |
| Not enough disk space                   | File (the link continues)                          |
| Connection lost while copying           | Link (remaining files abandoned)                   |
| Local write/delete failure              | File                                               |
| More than 4096 files on either side     | Link (no truncated listing, which would break Mirror) |

A partially downloaded file is removed on failure or cancellation (never a truncated file left on
the SD card).

---

## Features (summary)

- 100% offline config in `Samba Sync.toml` (TOML, vendored tomlc17 parser): servers, links,
  settings.
- Main screen listing the links with a progressive check on open (something new, errors).
- "Sync everything" in one press, run in sequence, with progress shown in the list.
- A failure doesn't stop the other links; it is persisted and flagged.
- Detail screen per link: config, check, last sync result, list of errors.
- Add / Mirror mode per link.

## Out of scope for v2

- Creating or editing links or servers from the console.
- Syncing a single link from the UI.
- Push SD → Samba, two-way sync.
- Extension/pattern filters.
- Scheduled / at-boot / background sync.
- Comparison by modification date or checksum.
- Resuming an interrupted file.

## Target platforms

`tg5040`, `tg5050` (TrimUI Brick / Smart Pro).

## Open questions

- File location: SD card root (`Samba Sync.toml`) or next to the pak
  (`Tools/<platform>/Samba Sync.pak/config.toml`)?
- Should a single link be syncable from the detail screen (A)? Excluded for now.
- Button mapping: implemented as A = Sync everything / Y = Details. The NextUI convention (A opens
  the selected item) would suggest the opposite — to be settled through use on the console.
- Checking a link is blocking (connect + recursive listing): the UI redraws between links but
  freezes during each one, and B only takes effect between two links. To be re-evaluated on a large
  real share; a dedicated thread would be the fix.
