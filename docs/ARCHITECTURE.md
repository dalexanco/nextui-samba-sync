# Architecture technique

Ce document détaille l'implémentation technique du pak. Pour la description
fonctionnelle (écrans, features), voir [SPEC.md](../SPEC.md). Pour une vue
d'ensemble rapide, voir [CLAUDE.md](../CLAUDE.md).

## Arborescence des sources

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
      home.c/.h, jobs_list.c/.h, servers_list.c/.h, job_wizard.c/.h,
      folder_browser.c/.h, preview.c/.h, progress.c/.h, error.c/.h,
      summary.c/.h, settings.c/.h
    browse.c/.h
    smb_client.c/.h
    sync_engine.c/.h
    servers.c/.h
    jobs.c/.h
    settings.c/.h
  lib/
    libsmb2/   (vendored git submodule)
  screenshots/
```

## Modèle de données et persistance

### Serveurs (config, lecture seule côté pak)

`Samba Servers/<nom>/server.txt`, un dossier par serveur, scanné par
`servers.c` via `servers_rescan()`. Entrées malformées ignorées silencieusement
(non fatal). Mot de passe gardé tel quel en mémoire.

```
name=NAS Salon
host=192.168.1.10
port=445
share=Roms
username=guest
password=
domain=
```

### Jobs

`$SHARED_USERDATA_PATH/samba-sync/jobs/<slug>.txt` :

```
name=GBA
server=NAS Salon
remote_path=Roms/GBA
local_path=Roms/Game Boy Advance (GBA)
mirror=0
last_sync_status=ok
last_sync_time=1757793000
```

Le serveur est référencé par `name` et résolu dynamiquement à chaque usage.
Si le serveur est absent (supprimé/renommé), le job passe en état d'erreur
(badge + écran 5) mais n'est jamais supprimé automatiquement.

### Réglages globaux

`$SHARED_USERDATA_PATH/samba-sync/settings.txt` :

```
overwrite_existing=0
preview_before_sync=1
network_timeout=10
```

Lecture/écriture en fopen/fscanf simple. On n'utilise pas
`PLAT_OpenSettings`/`PLAT_WriteSettings` (réservés à l'état niveau OS, pas
aux réglages d'un pak tiers) — comme dans gift-code.

## Machine à états des écrans

Un `enum Screen` avec une valeur par écran du SPEC.md. Boucle de rendu
"dirty"-gated (redraw seulement sur changement d'état). Chaque écran expose
`init` / `update(dt)` / `render`. Pas d'OOP/vtable : un simple `switch` dans
`main.c`, comme gift-code/Parental.pak.

## Modules clés

### `browse.c/.h`

Abstraction de navigation partagée entre l'écran 2b (distant) et l'écran 3
(local), via une interface de backend :

```c
typedef struct {
    int (*list)(void* ctx, const char* path, BrowseEntry** out, int* count);
    int (*is_dir)(const BrowseEntry* e);
} BrowseBackend;
```

Backend local via `opendir`/`readdir`/`stat` ; backend distant via
`smb2_opendir`/`readdir` à travers `smb_client.c`.

### `smb_client.c/.h`

Wrapper fin autour de libsmb2 :
- `smb_connect(server) -> SmbSession*`
- `smb_list(session, remote_path) -> entries[]`
- `smb_open_read` / `smb_read_chunk` (lecture par blocs de 64KB)
- `smb_disconnect`
- Mapping d'erreurs vers `enum SmbError`

### `sync_engine.c/.h`

Machine à états incrémentale, avancée à chaque tick (pas de thread) :

```
CONNECTING -> LISTING_REMOTE -> LISTING_LOCAL -> DIFFING
  -> [WAITING_CONFIRM] -> COPYING -> DELETING -> DONE
```

`sync_engine_tick()` avance d'un pas par appel (une entrée de listing, un
bloc de 64KB, une suppression). `sync_engine_cancel()` positionne un flag
vérifié à chaque tick. La file multi-job est gérée par une couche au-dessus
(`sync_queue.c` ou directement dans `home.c`). Détection "déjà présent" par
nom + taille uniquement (pas de checksum en v1).

### `servers.c/.h`, `jobs.c/.h`, `settings.c/.h`

I/O key=value pur (fopen/fgets/sscanf), tableaux en mémoire de taille fixe
(`MAX_SERVERS`, `MAX_JOBS`), pas d'allocation dynamique, pas de parseur
générique — même niveau de simplicité que `config.c` dans gift-code.

## Client SMB : pourquoi libsmb2

[libsmb2](https://github.com/sahlberg/libsmb2) (sahlberg/libsmb2) :
- LGPLv2.1
- ~50KB compilé
- Aucune dépendance hors libc (Kerberos optionnel, désactivé ici puisque
  v1 ne gère que NTLM/guest)
- Utilisé en production par RetroArch et Kodi

Alternative rejetée : `mount.cifs` / module CIFS noyau — le support CIFS du
noyau Buildroot du TrimUI n'est pas vérifié, trop risqué comme base.

Vendored en submodule git pinné dans `lib/libsmb2/`.

## Build & cross-compilation

Même squelette que gift-code :
- `build-desktop.sh` / `build-tg5040.sh` / `build-tg5050.sh` / `run-docker.sh`
- Symlink `.nextui-workspace`
- Toolchain Docker `ghcr.io/loveretro/${PLATFORM}-toolchain`
- `CROSS_COMPILE=aarch64-nextui-linux-gnu-`

Extensions au Makefile de gift-code :
1. Plusieurs fichiers `LOCAL_SRC` au lieu d'un seul.
2. `libsmb2` compilé séparément (son propre build CMake, GSSAPI désactivé)
   en `libsmb2.a` statique par plateforme, lié via `MY_LDFLAGS += -lsmb2`
   avec les `-L`/`-I` appropriés.

`build-desktop.sh` compile aussi libsmb2 nativement pour l'itération locale.

## Gestion des erreurs (mapping)

| Cause | Écran |
|---|---|
| Hôte injoignable / timeout | 5 |
| Authentification refusée (`STATUS_LOGON_FAILURE`) | 5 |
| Partage/chemin distant introuvable (`STATUS_OBJECT_NAME_NOT_FOUND` / `OBJECT_PATH_NOT_FOUND`) | 5 |
| Disque local plein / illisible (`ENOSPC` / `EACCES`) | 5 |
| Serveur référencé par un job introuvable | 5 |
| Erreurs non fatales par fichier | 5bis (résumé) |

## Stratégie de test

- `build-desktop.sh` comme outil principal d'itération.
- Conteneur Samba Docker jetable sur la machine de dev (Raspberry Pi
  homelab) pour tester le SMB réel, ex. :
  `docker run -p 445:445 -v $(pwd)/testshare:/share dperson/samba ...`
- Pas de tests automatisés prévus (comme gift-code).
- Validation manuelle : `build-desktop.sh` puis sur device via
  `build-tg5040.sh`/`build-tg5050.sh` + ADB avant chaque release.

## Risques techniques / points à valider en implémentation

- **Sync engine sans thread** : pas encore vérifié qu'un appel libsmb2
  unique ne puisse pas bloquer assez longtemps pour geler visiblement
  l'UI. Pourrait nécessiter des chunks plus petits, ou un vrai thread
  dédié avec passage de messages (plus proche de ce que fait probablement
  Mortar.pak en Go) si ça pose problème en pratique.
- **Cross-compilation aarch64 de libsmb2** non confirmée pour ce toolchain
  NextUI précis — à valider comme premier jalon indépendant avant d'écrire
  le reste de l'app.
- **Noms exacts des variables d'environnement** (`$SHARED_USERDATA_PATH`,
  `$SDCARD_PATH`) supposés stables (utilisés tels quels par gift-code) mais
  pas encore confirmés sur un vrai environnement NextUI.
