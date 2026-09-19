# tomlc17 (vendored)

TOML v1.1 parser used to read `Samba Sync.toml` (see `../../sync_config.c`).

- Upstream: https://github.com/cktan/tomlc17
- Commit: `e229ff59d235ab38e65b9114f57d7c19b46610f1` (2026-09-15)
- License: MIT (`LICENSE`)
- Files copied unmodified: `src/tomlc17.c`, `src/tomlc17.h`

Chosen over its predecessor tomlc99, whose README now declares it obsolete.
Table keys are exposed in file order (`u.tab.key[]`), which the pak relies on
for the display/sync order of links (see SPEC.md).

Built with `-std=gnu11` (it uses C11 `static_assert`), unlike the rest of the
pak which stays on `-std=gnu99` -- see the dedicated rule in `../../Makefile`.
