# Samba Sync for NextUI

A **NextUI** pak (Tool type) that pulls folders from a **Samba/SMB** share on
your local network and copies them onto the console's SD card.

Typical use case: you keep your ROMs, BIOS files or saves on a NAS, and you
want to update the console with a single button press, without taking the SD
card out.

> **Status: in development (v0.1).** The core works (job creation, sync,
> preview, summary), but some features are not wired up yet — see
> [Current limitations](#current-limitations).
>
> The console UI is currently in French; on-screen labels are quoted below in
> parentheses.

---

## Contents

- [How it works](#how-it-works)
- [Supported devices](#supported-devices)
- [Installation](#installation)
- [Server configuration](#server-configuration)
- [Usage](#usage)
- [Sync modes](#sync-modes)
- [Settings](#settings)
- [Where data is stored](#where-data-is-stored)
- [Current limitations](#current-limitations)
- [Building](#building)
- [Technical documentation](#technical-documentation)
- [Credits and licenses](#credits-and-licenses)

---

## How it works

- **Pull only**: the console downloads from the server. The remote share is
  **never modified**.
- **Jobs**: a job maps a folder on the share to a folder on the SD card
  (e.g. `Roms/GBA` on the NAS → `Roms/Game Boy Advance (GBA)` on the SD). You
  can create as many as you like.
- **No typing**: servers are declared in a text file on the SD card, and jobs
  are created entirely by browsing with the D-pad. The job name is derived
  from the chosen folder.
- **Fast re-runs**: a file already present locally with the same size is
  skipped; only new files are copied.

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

3. Declare at least one server (next section).
4. Launch **Samba Sync** from the NextUI Tools menu.

## Server configuration

Servers are declared **on a computer** with the SD card mounted (or over
SSH), in a `Samba Servers/` folder at the root of the card, with one
subfolder per server:

```
Samba Servers/
  Living Room NAS/
    server.txt
  Office NAS/
    server.txt
```

Each `server.txt` holds one `key=value` line per setting:

```
name=Living Room NAS
host=192.168.1.10
port=445
share=Roms
username=guest
password=
domain=
```

| Key        | Required | Description                                  |
|------------|:--------:|----------------------------------------------|
| `name`     | yes      | Name shown on the console                    |
| `host`     | yes      | Server IP address or hostname                |
| `share`    | yes      | SMB share name                               |
| `port`     | no       | SMB port (default: `445`)                    |
| `username` | no       | Username; empty = guest access               |
| `password` | no       | Password                                     |
| `domain`   | no       | Windows domain, if your network uses one     |

An incomplete `server.txt` (missing `name`, `host` or `share`) is simply
ignored: it doesn't show up in the list, and doesn't block the others.

> ⚠️ **Security**: the password is stored **in plain text** on the SD card,
> like any other text file. Avoid putting sensitive credentials there; prefer
> a dedicated read-only account on your NAS.

Supported protocols: SMB2/SMB3, NTLM or guest authentication (no Kerberos).

## Usage

### Home screen

| Button | Action                                                     |
|--------|------------------------------------------------------------|
| **A**  | Sync everything (*Synchroniser*) — all jobs, in order      |
| **Y**  | Manage jobs (*Gérer les jobs*)                             |
| **B**  | Quit (*Quitter*)                                           |

The home screen shows the number of jobs and the time of the last sync.

### Creating a job

From **Manage jobs**, press **X** (*Ajouter*), then:

1. **Pick a server** from the declared ones. The connection is tested right
   away.
2. **Browse the remote share** (A to enter a folder, B to go up) and press
   **Y** to select the source folder.
3. **Browse the SD card** and press **Y** to select the destination.
   **X** creates a subfolder here named after the remote folder.
4. **Summary**: **Y** toggles Mirror mode, **A** saves.

### Running a sync

- **All jobs**: A from the home screen. A failing job doesn't stop the
  following ones from running.
- **A single job**: A on that job in **Manage jobs**.

Before starting, a **preview** shows the number of files and the amount of
data to copy, along with the files that will be **deleted** (Mirror-mode
jobs). Nothing happens until you confirm.

While copying, the screen shows the current file, progress and amount
transferred. **B** cancels cleanly: whatever was already copied stays in
place. At the end, a summary lists copied, skipped and deleted files, and any
errors.

## Sync modes

| Mode                      | Copies new files | Deletes local files missing from the server |
|---------------------------|:----------------:|:-------------------------------------------:|
| **Add only** (default)    | ✔                | ✘                                           |
| **Mirror**                | ✔                | ✔                                           |

In **Mirror** mode, the local folder becomes an exact copy of the remote
folder. Deletions:

- **only** affect the job's destination folder, never anything outside it;
- happen **only after** the copy succeeded;
- are listed separately in the pre-sync preview.

## Settings

Open with **MENU** from **Manage jobs**.

| Setting                                                   | Default | Effect                                                  |
|-----------------------------------------------------------|---------|---------------------------------------------------------|
| Overwrite existing files (*Écraser les fichiers existants*) | No      | Yes = re-download even files already present            |
| Preview before syncing (*Vérifier avant de synchroniser*)   | Yes     | Shows the preview and asks for confirmation before sync |
| Network timeout, seconds (*Timeout réseau*)                 | 10      | Delay before giving up on an unresponsive connection    |
| View servers… (*Voir les serveurs…*)                        | —       | Lists declared servers and lets you test each one       |

> These three settings are saved but **not applied yet** by the sync engine
> (see below).

## Where data is stored

| Data                    | Location                                          | Hand-editable |
|-------------------------|---------------------------------------------------|:-------------:|
| Servers                 | `Samba Servers/<name>/server.txt` (SD root)       | yes           |
| Jobs (one file per job) | `.userdata/shared/samba-sync/<job>.txt`           | not advised   |
| Settings                | `.userdata/shared/samba-sync/settings.txt`        | not advised   |
| Run log                 | `sambasync.txt` in the NextUI logs folder         | —             |

## Current limitations

Planned but not available yet:

- **Editing and deleting a job** from the console. In the meantime, you can
  delete the job's file in `.userdata/shared/samba-sync/`.
- **Settings have no effect** for now: the preview is always shown, existing
  files are never overwritten, and the network timeout stays at its default.

Out of scope for v1:

- Syncing to the server (push) or two-way sync.
- Extension filters or exclusions.
- Automatic sync (at boot, scheduled, in the background).
- Resuming an interrupted sync where it left off.

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
docker run -p 445:445 -v "$PWD/testshare:/share" dperson/samba -s "share;/share"
```

### Code layout

```
src/
  main.c            main loop and screen transitions
  screens/          one file per screen (home, list, wizard, preview…)
  servers.c         reads Samba Servers/
  jobs.c            job storage
  settings.c        settings storage
  smb_client.c      SMB access (on top of libsmb2)
  sync_engine.c     compare, copy and delete for one job
  sync_queue.c      runs several jobs in a row ("Sync everything")
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
- Built for [NextUI](https://github.com/LoveRetro/NextUI).
