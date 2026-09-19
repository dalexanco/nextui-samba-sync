# nextui-samba-sync

NextUI pak (type TOOL) that syncs folders from a Samba/SMB share onto the SD
card. v2: pull only, several "links" declared offline, Add or Mirror mode
chosen per link.

Inspired by [nextui-gift-code](https://github.com/dalexanco/nextui-gift-code),
whose philosophy it keeps: minimal C, no superfluous dependencies, offline
config on the SD card, Docker cross-compile toolchain. Two departures from
that precedent: several source files instead of one, and two vendored
dependencies (libsmb2, tomlc17).

## Core principle: the config describes, the UI triggers

The pak offers no text entry, no virtual keyboard and no way to create
anything from the console. Everything that describes the sync — servers,
credentials, remote folder, local folder, mode — is declared in a single file
on the SD card (`Samba Sync.toml`, TOML format), edited from a computer.

A **link** = one folder of an SMB share tied to one folder of the SD card.
The UI is limited to two screens: the list of links (checked one by one when
the pak opens, all of them synced with one press, progress shown in the list)
and one link's detail (config, check, last sync and its errors).

## Technical stack

- C, SDL2, shared NextUI API (`GFX_*`, `PAD_*`) via `workspace/all/common/`
- [libsmb2](https://github.com/sahlberg/libsmb2) vendored as a submodule for
  the SMB2/3 client (static, Kerberos disabled, NTLM/guest only)
- [tomlc17](https://github.com/cktan/tomlc17) copied into `src/vendor/tomlc17/`
  to read the config (built as `-std=gnu11`, the rest of the pak as `gnu99`)
- Cross-compiled through the `ghcr.io/loveretro/${PLATFORM}-toolchain` Docker
  toolchain, targeting `tg5040` / `tg5050`

## Going further

- [SPEC.md](SPEC.md) — functional spec: config, screens, flows, error cases,
  out of scope
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — detailed technical
  architecture: source tree, data model, key modules, build, error handling,
  test strategy, technical risks
