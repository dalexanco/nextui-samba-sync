# Samba Sync — Architecture technique

Pak (type `TOOL`) pour NextUI (TrimUI Brick/Smart Pro, `tg5040`/`tg5050`), qui synchronise en pull
des dossiers depuis des partages Samba déclarés hors-ligne vers la carte SD. Voir [`SPEC.md`](SPEC.md)
pour les écrans et features côté utilisateur — ce document couvre l'implémentation.

Construit dans le même esprit que [`nextui-gift-code`](https://github.com/dalexanco/nextui-gift-code) :
C, cross-compilation via le toolchain Docker de NextUI, réglages en `key=value`, aucune dépendance
lourde. Deux écarts assumés par rapport à ce précédent, documentés ci-dessous : le code est réparti
sur plusieurs fichiers (la complexité — client SMB, assistant à plusieurs écrans, moteur de sync
incrémental — dépasse ce qui tient raisonnablement dans un seul fichier), et une bibliothèque est
vendorisée (`libsmb2`, SMB2/3 ne se réimplémente pas à la main).

## Arborescence des sources

```
nextui-samba-sync/
  pak.json
  launch.sh
  build-desktop.sh / build-tg5040.sh / build-tg5050.sh / run-docker.sh
  .nextui-workspace          (symlink, one-time setup, comme gift-code)
  src/
    Makefile
    main.c                # entrée, boucle principale, machine à états des écrans
    screens/
      home.c/.h              # écran 0 — accueil simplifié / Tout synchroniser
      jobs_list.c/.h         # écran 1 — gestion des jobs
      servers_list.c/.h      # écran 1bis — serveurs (lecture seule)
      job_wizard.c/.h        # écrans 2a/2b/2c — choisir serveur / parcourir distant / récap
      folder_browser.c/.h    # écran 3 — parcourir la SD (partagé assistant + usage futur)
      preview.c/.h           # écran 3bis — aperçu avant sync
      progress.c/.h          # écran 4 — progression
      error.c/.h             # écran 5 — erreur
      summary.c/.h           # écran 5bis — résumé
      settings.c/.h          # écran 6 — réglages globaux
    browse.c/.h            # abstraction de navigation (backend local FS ou SMB distant)
    smb_client.c/.h        # wrapper fin autour de libsmb2
    sync_engine.c/.h       # diff + copie + suppression, incrémental, pilotable frame par frame
    servers.c/.h           # scan + parsing de Samba Servers/*/server.txt
    jobs.c/.h              # CRUD + (dé)sérialisation des jobs internes
    settings.c/.h          # réglages globaux (key=value)
  lib/
    libsmb2/               # vendorisé en submodule git, pinné sur un tag de release
  screenshots/
```

Pas de framework UI, pas de moteur d'état générique : même style que gift-code, juste plus de
fichiers pour séparer les responsabilités (I/O réseau, moteur de sync, et rendu de chaque écran).

## Modèle de données et persistance

### `Samba Servers/<nom>/server.txt` — déclaratif, édité hors-ligne

Format et emplacement détaillés dans `SPEC.md`. Côté implémentation :

- `servers.c` scanne `SDCARD_PATH/Samba Servers/*/server.txt` au lancement du pak, dans une liste
  fixe (`Server servers[MAX_SERVERS]`), même approche que `scanGifts()` dans gift-code.
- Un fichier manquant un champ obligatoire (`name`, `host`, `share`) est **sauté**, pas fatal.
  Un compteur d'entrées ignorées peut être loggé (`$LOGS_PATH`) mais pas affiché à l'utilisateur.
- Rechargeable à la demande (écran 1bis, touche X) sans redémarrer le pak — `servers.c` expose
  `servers_rescan()`, appelée aussi bien au démarrage qu'à la demande.
- Le mot de passe est lu et gardé en mémoire tel quel (pas de hash/obfuscation — cohérent avec le
  choix de le stocker en clair, documenté dans `SPEC.md`).

### Jobs — persistés par le pak, `$SHARED_USERDATA_PATH/samba-sync/jobs/<slug>.txt`

```
name=GBA
server=NAS Salon
remote_path=Roms/GBA
local_path=Roms/Game Boy Advance (GBA)
mirror=0
last_sync_status=ok
last_sync_time=1757793000
```

- Un fichier par job (comme les serveurs), nommé d'après un slug dérivé du nom du job (ex.
  `gba.txt`), avec suffixe numérique en cas de collision — jamais exposé à l'utilisateur.
- `server` référence un serveur **par son champ `name`**, résolu dynamiquement à chaque usage
  (lancement d'une sync, affichage de la liste) en cherchant dans la liste chargée par `servers.c`.
  Si absent → le job s'affiche avec un badge d'erreur et route vers l'écran 5 au lancement, sans
  jamais être supprimé automatiquement (l'utilisateur peut corriger `Samba Servers/` ou réassigner
  le job à un autre serveur via l'assistant en mode édition).
- `last_sync_status` / `last_sync_time` mis à jour après chaque exécution (individuelle ou via
  "Tout synchroniser"), lus par l'écran 1 (badges) et l'écran 0 (résumé global agrégé).

### Réglages globaux — `$SHARED_USERDATA_PATH/samba-sync/settings.txt`

```
overwrite_existing=0
preview_before_sync=1
network_timeout=10
```

Mêmes conventions que `giftcode.txt` : `fopen`/`fscanf` direct sur `key=value`, pas de lib de
parsing, pas de PLAT_OpenSettings/PLAT_WriteSettings (ces fonctions gèrent l'état système de
NextUI, pas les réglages propres à un pak tiers — gift-code ne les utilise pas non plus pour son
propre fichier).

## Machine à états des écrans

Un `enum Screen` avec une valeur par écran de `SPEC.md` (`SCREEN_HOME`, `SCREEN_JOBS_LIST`,
`SCREEN_SERVERS`, `SCREEN_WIZARD_SERVER`, `SCREEN_WIZARD_REMOTE_BROWSE`,
`SCREEN_WIZARD_LOCAL_BROWSE`, `SCREEN_WIZARD_SUMMARY`, `SCREEN_PREVIEW`, `SCREEN_PROGRESS`,
`SCREEN_ERROR`, `SCREEN_SUMMARY`, `SCREEN_SETTINGS`). Boucle principale identique au pattern
`Parental.pak`/gift-code : `dirty`-gated render (on ne redessine que si l'état a changé),
`PAD_update()` puis `PWR_update()` à chaque frame, `GFX_flip()` en fin de frame.

Chaque écran expose la même interface minimale (`init`, `update(dt) -> next_screen_or_self`,
`render`), pas de vtable/OO — un simple `switch` sur l'enum dans `main.c`, cohérent avec le style C
du reste de l'écosystème NextUI.

## Modules clés

### `browse` — navigateur générique (local et distant)

Les écrans 2b (parcourir le partage distant) et 3 (parcourir la SD) partagent la même logique de
navigation (liste d'entrées, entrer dans un dossier, remonter, sélectionner), seule la source des
entrées change. `browse.c` définit une petite interface à deux implémentations :

```c
typedef struct {
    int (*list)(void* ctx, const char* path, BrowseEntry** out, int* count);
    int (*is_dir)(const BrowseEntry* e);
} BrowseBackend;
```

- Backend local : `opendir`/`readdir`/`stat` sur la carte SD.
- Backend distant : `smb2_opendir`/`smb2_readdir` via `smb_client.c`, sur la session ouverte pour
  le serveur choisi à l'écran 2a.

Ça évite de dupliquer l'écran de navigation (rendu, gestion des touches, tri, troncature des noms
longs) entre les deux usages.

### `smb_client` — wrapper autour de libsmb2

Fine couche au-dessus de l'API de libsmb2, qui expose uniquement ce dont le pak a besoin :

- `smb_connect(server) -> SmbSession*` — résout host/port/share/credentials depuis un `Server`,
  ouvre la session (`smb2_connect_share`).
- `smb_list(session, remote_path) -> entries[]` — un niveau de listing (pour le navigateur distant
  ET pour construire l'arbre complet côté moteur de sync, appelé récursivement).
- `smb_open_read(session, remote_file)` / `smb_read_chunk(...)` — lecture par blocs (64 Ko) pour
  la copie, sans charger un fichier entier en mémoire.
- `smb_disconnect(session)`.
- Mapping des codes retour libsmb2 (`-ECONNREFUSED`, `-ETIMEDOUT`, NT status d'auth, etc.) vers un
  `enum SmbError` consommé par les écrans Erreur/Aperçu, indépendant des détails de libsmb2.

### `sync_engine` — diff, copie, suppression, incrémental

Le point le plus délicat : la sync ne doit **jamais bloquer la boucle UI** (pas de thread — cf.
section Risques), donc elle est modélisée comme une machine à états avancée à chaque frame :

```
CONNECTING → LISTING_REMOTE → LISTING_LOCAL → DIFFING
  → (si preview_before_sync) WAITING_CONFIRM
  → COPYING (une tranche de fichier par tick) → DELETING (mode Miroir, si activé) → DONE
```

- Chaque appel `sync_engine_tick()` fait progresser d'un pas (une entrée de listing, un bloc de
  64 Ko copié, un fichier supprimé) et retourne un statut + des stats à jour (copiés/ignorés/
  supprimés/erreurs/volume), consommées telles quelles par l'écran Progression.
- Annulation (`sync_engine_cancel()`) : positionne un flag vérifié à chaque tick, ferme la session
  SMB proprement, ne touche pas aux suppressions déjà faites ni ne les complète.
- Mode file ("Tout synchroniser") : une couche au-dessus (`sync_queue.c` ou géré directement dans
  `home.c`) enchaîne les jobs, un `sync_engine` par job, erreurs mémorisées sans interrompre la
  file — cf. `SPEC.md`.
- Détection "déjà présent" : comparaison nom + taille sur les deux arbres en mémoire (pas de
  checksum en v1), avec `overwrite_existing` qui force la copie même si identique.

### `servers` / `jobs` / `settings`

I/O fichier pur (`fopen`/`fgets`/`sscanf` sur des lignes `key=value`), pas de parseur générique —
même niveau de simplicité que `config.c` de gift-code. Chacun expose un tableau fixe en mémoire
(`MAX_SERVERS`, `MAX_JOBS`) plutôt qu'une liste chaînée, cohérent avec le style "pas d'allocation
dynamique inutile" du reste de l'écosystème.

## Client SMB : pourquoi libsmb2

- Bibliothèque userspace SMB2/3 (`sahlberg/libsmb2`, LGPLv2.1), ~50 Ko compilée, aucune dépendance
  hors libc (le support Kerberos est optionnel et sera désactivé à la compilation — v1 ne gère que
  l'auth NTLM/guest, pas de SSO). Utilisée par RetroArch, Kodi et d'autres projets de l'écosystème
  retrogaming pour exactement ce cas d'usage.
- Alternative écartée : monter le partage via le module noyau CIFS (`mount.cifs`). Non retenue car
  rien ne garantit que le noyau Buildroot du firmware TrimUI embarque `CONFIG_CIFS` ni que
  `cifs-utils` soit présent — dépendance invérifiable sans accès physique à la console, alors que
  libsmb2 fonctionne quel que soit le noyau.
- Vendorisée en `lib/libsmb2/` (submodule git, pinné sur un tag de release, pas sur une branche).

## Build & cross-compilation

Même squelette que gift-code (`build-desktop.sh`, `build-tg5040.sh`, `build-tg5050.sh`,
`run-docker.sh`, symlink `.nextui-workspace`), toolchain `ghcr.io/loveretro/${PLATFORM}-toolchain`,
`CROSS_COMPILE=aarch64-nextui-linux-gnu-`.

Le `Makefile` (`src/Makefile`) suit le même schéma que celui de gift-code — `NEXTUI_ALL`/
`NEXTUI_PLATFORM` résolus via `.nextui-workspace`, sources communes NextUI (`utils.c api.c
config.c scaler.c`) compilées à part des sources locales — étendu sur deux points :

1. **Sources locales multiples** : `LOCAL_SRC` liste tous les fichiers de `src/` (`main.c`,
   `screens/*.c`, `browse.c`, `smb_client.c`, `sync_engine.c`, `servers.c`, `jobs.c`,
   `settings.c`) au lieu d'un seul fichier.
2. **Lien avec libsmb2** : compilé séparément (son propre système de build, CMake, invoqué avec le
   même `CROSS_COMPILE`/sysroot que le reste, GSSAPI désactivé) en une lib statique
   `libsmb2.a` par plateforme, puis `MY_LDFLAGS += -lsmb2` avec le bon `-L`/`-I` vers
   `lib/libsmb2/{lib,include}`. Pas de lien dynamique — pas de `.so` à embarquer/déployer avec le
   pak.

`build-desktop.sh` compile aussi `libsmb2` nativement (pas besoin du toolchain Docker pour ça en
local) avant le premier lancement, même logique que le "one-time setup" de gift-code.

## Gestion des erreurs (mapping)

| Cause | Détection | Écran |
|---|---|---|
| Hôte injoignable / timeout | `smb2_connect_share` échoue, `errno`/timeout configuré | 5 |
| Authentification refusée | NT status `STATUS_LOGON_FAILURE` | 5 |
| Partage/dossier distant introuvable | `STATUS_OBJECT_NAME_NOT_FOUND`/`OBJECT_PATH_NOT_FOUND` | 5 |
| Disque local plein/illisible | `ENOSPC`/`EACCES` sur l'écriture locale | 5 |
| Serveur référencé par un job absent de `servers[]` | résolution du champ `server` échoue | 5 |
| Erreur ponctuelle sur un fichier (pendant une sync globalement OK) | erreur locale à une entrée, n'interrompt pas le job | 5bis (liste détaillée) |

## Stratégie de test

- `build-desktop.sh` reste le principal outil d'itération (UI, moteur de sync, parsing) sans passer
  par le toolchain Docker à chaque changement.
- Pour tester réellement le client SMB, le plus simple est un petit serveur Samba jetable sur la
  même machine de dev (ex. `docker run -p 445:445 -v $(pwd)/testshare:/share dperson/samba ...`) —
  faisable directement sur le Pi du homelab, pas besoin d'un NAS dédié.
- Pas de tests automatisés prévus en v1 (aucun ne l'est dans gift-code non plus) — validation
  manuelle via `build-desktop.sh` contre ce serveur de test, puis sur device via
  `build-tg5040.sh`/`build-tg5050.sh` + ADB avant chaque release.

## Risques techniques / points à valider en implémentation

- **Pas de threads** : le choix d'un moteur de sync "tick par frame" suppose que lire un bloc de
  64 Ko ou lister un dossier distant ne bloque jamais assez longtemps pour geler l'UI de façon
  gênante. À valider avec un vrai réseau lent/instable — si un seul appel libsmb2 peut bloquer
  plusieurs centaines de ms, il faudra soit réduire la taille des blocs, soit revoir vers un thread
  dédié avec passage de messages (plus proche de ce que ferait Mortar.pak en Go).
- **Compatibilité aarch64 de libsmb2** : la lib compile large (Linux desktop, PSVita, RP2040 selon
  sa doc) mais n'a pas de precedent confirmé sur ce toolchain NextUI précis — premier essai de
  cross-compilation à traiter comme une étape à part, avant d'écrire le reste du code applicatif.
- **`$SHARED_USERDATA_PATH` / `$SDCARD_PATH`** : à confirmer précisément (noms exacts des variables
  d'env exposées par `launch.sh`/le runtime NextUI) une fois dans le contexte réel du workspace
  NextUI — gift-code les utilise telles quelles, donc très probablement stables, mais à vérifier
  avant de coder en dur les chemins.
