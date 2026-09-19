# Samba Sync for NextUI

A **NextUI** pak (Tool type) that pulls folders from a **Samba/SMB** share on
your local network and copies them onto the console's SD card.

Typical use case: you keep your ROMs, BIOS files or saves on a NAS, and you
want to update the console with a single button press, without taking the SD
card out.

> **Status: in development (v0.2).** What to sync is declared in one
> configuration file on the SD card; the console UI only checks, triggers and
> reports. Not yet validated on a real device — see
> [Current limitations](#current-limitations).
>
> The console UI is in French; on-screen labels are quoted below in
> parentheses.

---

## Contents

- [How it works](#how-it-works)
- [Supported devices](#supported-devices)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Sync modes](#sync-modes)
- [Where data is stored](#where-data-is-stored)
- [Current limitations](#current-limitations)
- [Building](#building)
- [Technical documentation](#technical-documentation)
- [Credits and licenses](#credits-and-licenses)

## How it works

- **Pull only**: the console downloads from the server. The remote share is
  **never modified**.
- **Links** (*liaisons*): a link maps a folder on a share to a folder on the SD
  card (e.g. `Roms/GBA` on the NAS → `Roms/Game Boy Advance (GBA)` on the SD).
  You can declare as many as you like.
- **No typing on the console**: everything — servers, folders, mode — lives in
  a single `Samba Sync.toml` file that you edit on a computer.
- **Checked on open**: when the pak starts, each link is checked in turn and
  reports how many files are new, how many would be deleted, or what went
  wrong.
- **Fast re-runs**: a file already present locally with the same size is
  skipped; only new or changed files are copied.

## Supported devices

| NextUI platform | Devices          |
|-----------------|------------------|
| `tg5040`        | TrimUI Brick     |
| `tg5050`        | TrimUI Smart Pro |

The console must be connected to Wi-Fi, on the same network as the Samba
server.

## Installation

1. Get the `Samba.Sync.pak.zip` archive (or build the pak, see below).
2. Extract it onto the SD card under `Tools/<platform>/`, for example:

   ```
   Tools/tg5040/Samba Sync.pak/
     launch.sh
     pak.json
     bin/tg5040/sambasync.elf
   ```

3. Create `Samba Sync.toml` at the root of the SD card (next section).
4. Launch **Samba Sync** from the NextUI Tools menu.

## Configuration

Everything is declared in **one file at the root of the SD card**, named
`Samba Sync.toml`, edited with the card mounted on a computer (or over SSH).
It is [TOML](https://toml.io/en/): keys are `key = value`, strings need
quotes, and `#` starts a comment.

```toml
[settings]
timeout = 10

[servers."Living Room NAS"]
host = "192.168.1.10"
port = 445
username = "tester"
password = "secret"

[links."Roms GBA"]
server = "Living Room NAS"
share = "Roms"
remote = "GBA"
local = "Roms/Game Boy Advance (GBA)"
mode = "mirror"

[links."Bios"]
server = "Living Room NAS"
share = "Roms"
remote = "System/Bios"
local = "Bios"
```

### `[servers."<name>"]`

One table per server. The name in quotes is how links refer to it, and what
the console displays.

| Key        | Required | Description                              |
|------------|:--------:|------------------------------------------|
| `host`     | yes      | Server IP address or hostname            |
| `port`     | no       | SMB port (default: `445`)                |
| `username` | no       | Username; empty or absent = guest access |
| `password` | no       | Password                                 |
| `domain`   | no       | Windows domain, if your network uses one |

### `[links."<name>"]`

One table per link. The name in quotes is shown on the console and is also how
its history is stored, so renaming a link loses its last-sync record.

| Key      | Required | Description                                                     |
|----------|:--------:|-----------------------------------------------------------------|
| `server` | yes      | Name of a `[servers."…"]` table                                  |
| `share`  | yes      | SMB share name                                                   |
| `remote` | no       | Folder inside the share; empty or absent = the share root        |
| `local`  | yes      | Destination folder, relative to the SD card root; created if missing |
| `mode`   | no       | `"add"` (default) or `"mirror"` — see [Sync modes](#sync-modes)  |

### `[settings]`

| Key       | Default | Description                                      |
|-----------|:-------:|--------------------------------------------------|
| `timeout` | `10`    | Seconds before giving up on an unresponsive server |

### If something is wrong in the file

- A **syntax error** means no link can be read: the console shows the error
  with its line number.
- A **bad link** (missing `share`, unknown `server`, invalid `mode`, a `local`
  path escaping the SD card…) is listed on the console with the reason, and the
  other links keep working.
- Two links (or two servers) with the **same name** is a TOML error, and makes
  the whole file unreadable.
- Unknown keys and tables are ignored.

Links are displayed and synced **in the order they appear in the file**. That
order comes from the TOML parser rather than the TOML standard, so a tool that
reformats or sorts the file may change it.

> ⚠️ **Security**: the password is stored **in plain text** on the SD card,
> like any other text file. Avoid putting sensitive credentials there; prefer
> a dedicated read-only account on your NAS.

Supported protocols: SMB2/SMB3, NTLM or guest authentication (no Kerberos).

## Usage

The pak has two screens.

### Main screen — the list of links

Checking starts as soon as the pak opens, one link after another. Each line
shows the link name, `[Miroir]` if it is in mirror mode, and its status:

| Status                          | Meaning                                       |
|---------------------------------|-----------------------------------------------|
| `En attente` / `Vérification…`  | Waiting its turn / being checked              |
| `12 nouveaux · 3 à supprimer`   | Changes to apply (deletions in mirror mode)   |
| `À jour`                        | Nothing to copy or delete                     |
| `Erreur : …`                    | The check failed (network, credentials, path) |
| `Config : …`                    | The file is wrong for this link               |

A `!` before the name means this link's **last sync** failed or was partial.
The bottom line shows when the last sync ran and how many links failed.

| Button | Action                                                           |
|--------|------------------------------------------------------------------|
| **A**  | Sync everything (*Synchro*) — all valid links, in order          |
| **X**  | Re-read the file and check again (*Vérif.*)                      |
| **Y**  | Open the selected link's detail (*Détail*)                       |
| **B**  | Quit (*Quitter*), or cancel during a sync (*Annuler*)            |

### Running a sync

Press **A**. Links are synced one after another, on this same screen: the
active line shows the file being copied, the count and the percentage. A
failing link never stops the ones after it — its error is recorded and shown.

**B** cancels the current link and all remaining ones. Whatever was already
copied stays in place, and no partly-downloaded file is ever left behind.

### Detail screen

Press **Y** on a link. It shows its configuration, the result of the last
check, and the last sync: date, status, files copied and deleted, and the list
of errors (up to 50, with the total). Use up/down to scroll, **B** to go back.

## Sync modes

| Mode                       | Copies new and changed files | Deletes local files missing from the server |
|----------------------------|:----------------------------:|:-------------------------------------------:|
| **Add** (`"add"`, default) | yes                          | no                                          |
| **Mirror** (`"mirror"`)    | yes                          | yes                                         |

In **Mirror** mode the local folder becomes an exact copy of the remote
folder. Deletions:

- **only** affect that link's destination folder, never anything outside it;
- happen **only after** every file copied successfully — if any file failed,
  nothing is deleted;
- are counted on the main screen after a check (`3 à supprimer`), before you
  start the sync.

Files are compared by **name and size**: a remote file modified without any
size change is not re-copied.

## Where data is stored

| Data                | Location                                             | Hand-editable |
|---------------------|------------------------------------------------------|:-------------:|
| Servers and links   | `Samba Sync.toml` (SD card root)                     | yes           |
| Last sync per link  | `.userdata/shared/samba-sync/state/<link>.txt`       | not advised   |
| Run log             | `sambasync.txt` in the NextUI logs folder            | —             |

## Current limitations

- **Not yet tested on a real console**: verified on the desktop build against a
  throwaway Samba server.
- **Checking blocks the UI** while a link is being checked: the screen updates
  between links, and **B** only takes effect between two links.
- **Syncing a single link** from the UI isn't available; **A** always syncs
  everything (links already up to date pass through quickly).
- At most **4096 files per side** of a link; beyond that the link reports an
  error instead of syncing a partial listing.

Out of scope:

- Creating or editing links from the console.
- Syncing to the server (push) or two-way sync.
- Extension filters or exclusions.
- Automatic sync (at boot, scheduled, in the background).
- Resuming an interrupted file where it left off.

## Building

### Requirements

- Docker (for cross-compiling to the console)
- A checkout of [NextUI](https://github.com/LoveRetro/NextUI)
- `adb` to push the binary to the console (optional)

### Setup

```sh
git clone --recursive https://github.com/dalexanco/nextui-samba-sync.git
cd nextui-samba-sync

# Link to the workspace/ folder of your NextUI checkout
ln -s /path/to/NextUI/workspace .nextui-workspace
```

If you cloned without `--recursive`, fetch libsmb2 with
`git submodule update --init`.

### Build

| Command                          | Result                                                        |
|----------------------------------|---------------------------------------------------------------|
| `sh build-tg5040.sh`             | Cross-compiles for the TrimUI Brick (Docker) and pushes via ADB |
| `sh build-tg5050.sh`             | Same for the TrimUI Smart Pro                                 |
| `sh build-desktop.sh`            | Builds and runs the pak on your machine (Linux/macOS)         |
| `sh build-desktop.sh --build`    | Build only, don't run                                         |

The binary is written to `bin/<platform>/sambasync.elf`. libsmb2 is built
once per platform into `lib/build/`.

To test without a NAS, a throwaway Samba container is enough, for example:

```sh
docker run -d -p 1445:445 -v "$PWD/testshare:/share" dperson/samba \
  -u "tester;secret" -s "Roms;/share;yes;no;no;tester" -p
```

### Code layout

```
src/
  main.c            main loop and screen transitions
  screens/          the two screens (links_list, link_detail)
  sync_config.c     reads Samba Sync.toml
  link_state.c      last sync result, per link
  sync_queue.c      runs the check or the sync across links
  sync_engine.c     check, copy and delete for one link
  smb_client.c      SMB access (on top of libsmb2)
  local_fs.c        listing and folder creation on the SD card
  vendor/tomlc17/   vendored TOML parser
lib/
  libsmb2/          libsmb2 (git submodule)
  build-libsmb2.sh  static per-platform build of libsmb2
```

## Technical documentation

These documents are in French:

- [SPEC.md](SPEC.md): detailed functional spec (screens, flows, behavior).
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): architecture, data model,
  build, error handling, technical risks.

## Credits and licenses

- Inspired by [nextui-gift-code](https://github.com/dalexanco/nextui-gift-code).
- SMB client: [libsmb2](https://github.com/sahlberg/libsmb2) by Ronnie
  Sahlberg, LGPL-2.1, statically linked.
- TOML parser: [tomlc17](https://github.com/cktan/tomlc17) by CK Tan, MIT,
  vendored in `src/vendor/tomlc17/`.
- Built for [NextUI](https://github.com/LoveRetro/NextUI).
