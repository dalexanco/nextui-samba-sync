# Architecture technique

Ce document détaille l'implémentation technique du pak. Pour la description
fonctionnelle (écrans, flux), voir [SPEC.md](../SPEC.md). Pour une vue
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
      links_list.c/.h        (écran 1)
      link_detail.c/.h       (écran 2)
    sync_config.c/.h         (lecture de Samba Sync.toml)
    link_state.c/.h          (dernière synchro persistée)
    sync_queue.c/.h          (file de vérification / de synchro)
    sync_engine.c/.h         (vérification et synchro d'une liaison)
    smb_client.c/.h          (wrapper libsmb2)
    local_fs.c/.h            (listing et création côté SD)
    browse.c/.h              (BrowseFileEntry + helpers de chemin)
    ui.c/.h                  (helpers de rendu partagés)
    vendor/tomlc17/          (parseur TOML vendoré, voir VENDORED.md)
  lib/
    libsmb2/                 (submodule git)
    build-libsmb2.sh
```

Le module de config s'appelle `sync_config` et non `config` : `-I.` place `src/`
avant `all/common/`, donc un `config.h` local masquerait celui de NextUI, inclus
par `api.c`.

## Modèle de données et persistance

### Configuration (lecture seule côté pak)

`SDCARD_PATH "/Samba Sync.toml"`, lu par `sync_config_load()`. Trois tables :
`[settings]`, `[servers."<nom>"]`, `[links."<nom>"]` — clés et valeurs
énumérées en anglais, format détaillé dans [SPEC.md](../SPEC.md).

En mémoire : deux tableaux fixes (`MAX_SERVERS`, `MAX_LINKS`), pas d'allocation
dynamique. Une liaison invalide est **conservée** avec son `config_error`
rempli (message français affiché tel quel), pour que l'écran 1 puisse la
montrer en erreur au lieu de la faire disparaître silencieusement. Un fichier
absent ou du TOML mal formé donne zéro liaison et un `ConfigStatus` que l'écran
1 traduit en état vide.

L'ordre d'affichage et d'exécution est l'ordre des tables dans le fichier :
c'est ce que restitue `toml_datum_t.u.tab.key[]` de tomlc17, pas une garantie
de la norme TOML (voir `src/vendor/tomlc17/VENDORED.md`).

### État persistant

`$SHARED_USERDATA_PATH/samba-sync/state/<slug>.txt`, un fichier `key=value` par
liaison, écrit par `link_state.c` :

```
name=Roms GBA
status=partial
time=1758318000
copied=84
bytes=1288490188
deleted=5
errors=2
error=Espace disque insuffisant	Pokemon Ruby (USA).gba
error=Écriture refusée	Zelda Minish Cap (EU).gba
```

Le `name` est relu pour ignorer un fichier appartenant à une autre liaison dont
le nom donnerait le même slug. Les lignes `error=` portent la raison puis le
chemin, séparés par une tabulation (les raisons n'en contiennent jamais, les
chemins peuvent contenir n'importe quoi d'autre) ; au plus 50 sont gardées,
`errors=` donne le total réel.

## Machine à états des écrans

Deux écrans (`enum Screen` dans `main.c`), chacun exposant
`input(&dirty)` / `render(screen, show_setting)` — un `switch`, pas de vtable.
Boucle de rendu « dirty »-gated : seul `LinksList_input()` fait avancer la file,
donc une vérification en cours se met naturellement en pause tant que l'écran 2
est ouvert.

## Modules clés

### `sync_engine.c/.h`

`sync_engine_check(link)` est **bloquant** : connexion, listing récursif
distant, listing récursif local, puis diff. Les deux listes sont triées
(`qsort` sur `rel_path`) et fusionnées en une passe — le v1 comparait chaque
fichier distant à chaque fichier local, soit jusqu'à 16 millions de `strcmp`
à 4096 fichiers par côté.

`sync_engine_start(link)` refait le check (les suppressions Miroir doivent
reposer sur un état à jour) puis `sync_engine_tick()` avance par tranches :

```
COPYING -> [DELETING] -> DONE
        \-> CANCELLED
```

Chaque tick travaille pendant `TICK_BUDGET_MS` (30 ms) au lieu d'un seul bloc
de 64 Ko : à 60 fps, un bloc par tick plafonnait le débit à ~3,8 Mo/s.

Règles d'erreur :
- une erreur **par fichier** (ouverture, écriture, espace disque, suppression)
  est enregistrée et la liaison continue → statut `partial` ;
- une erreur **de liaison** (connexion perdue, serveur injoignable) arrête la
  liaison → statut `error` ;
- chaque fichier est écrit dans un `.part` masqué (préfixé d'un point, donc
  ignoré par `hide()` et par NextUI) renommé seulement une fois complet, et
  supprimé en cas d'échec ou d'annulation : jamais de fichier tronqué sur la SD ;
- en mode Miroir, les suppressions n'ont lieu que si **aucune** copie n'a
  échoué ; les dossiers devenus vides sont retirés ensuite, sans jamais sortir
  du dossier local de la liaison ;
- un chemin local qui ne tiendrait pas dans `MAX_PATH` est une erreur de
  fichier, pas un chemin tronqué qui désignerait un autre fichier.

### `sync_queue.c/.h`

Au-dessus de `sync_engine`, parcourt les liaisons valides en séquence, en mode
vérification ou synchro. Un échec n'arrête jamais la file. Le passage d'une
liaison à `CHECKING`/`SYNCING` consomme un tick à lui seul, pour que l'écran
affiche « Vérification… » **avant** l'appel bloquant. Conserve par liaison la
phase d'affichage, le dernier `LinkCheck` et le dernier `LinkState`.

### `smb_client.c/.h`

Wrapper fin autour de libsmb2 : `smb_connect` (le partage vient de la liaison),
`smb_list_files_recursive`, `smb_open_read`/`smb_read_chunk`/`smb_close_read`.
Les erreurs sont typées à partir de `smb2_get_nterror()` (`LOGON_FAILURE` →
auth, `BAD_NETWORK_NAME` → partage, `OBJECT_*_NOT_FOUND` → dossier, absence de
statut NT → serveur injoignable) et `smb_error_label()` en donne le libellé
français affiché.

Le listing distant applique la même règle `hide()` que le listing local :
sans cela un `.DS_Store` distant apparaîtrait indéfiniment comme « nouveau ».
Dépasser `BROWSE_MAX_FILES` est une erreur explicite : un listing tronqué
ferait supprimer, en mode Miroir, des fichiers qui existent à distance.

### `ui.c/.h`

Helpers de rendu partagés. `UI_fitText()` tronque sur les frontières UTF-8,
contrairement à `GFX_truncateText()` de NextUI qui retire 4 octets à la fois et
peut couper une lettre accentuée en deux.

Le texte est toujours posé sur une pastille (`ASSET_BLACK_PILL`) : la couleur
de fond est un réglage de thème NextUI (`COLOR_BACKGROUND`), donc du texte posé
directement sur le fond n'a pas de contraste garanti.

Contrainte de police : `font2.ttf` ne contient ni `✔` ni `✘`, et aucune des
deux polices livrées n'a `⟳`, `🗑` ou `⚠`. Les états sont donc écrits en toutes
lettres.

## Client SMB : pourquoi libsmb2

[libsmb2](https://github.com/sahlberg/libsmb2) (sahlberg/libsmb2) :
- LGPLv2.1
- ~50KB compilé
- Aucune dépendance hors libc (Kerberos désactivé explicitement, voir plus bas)
- Utilisé en production par RetroArch et Kodi

Alternative rejetée : `mount.cifs` / module CIFS noyau — le support CIFS du
noyau Buildroot du TrimUI n'est pas vérifié, trop risqué comme base.

Vendored en submodule git pinné dans `lib/libsmb2/`.

## Parseur TOML : tomlc17

Copié tel quel dans `src/vendor/tomlc17/` (MIT, un `.c` + un `.h`, aucune étape
de build séparée), voir `VENDORED.md` pour le commit épinglé. Choisi plutôt que
tomlc99, dont le README déclare la bibliothèque obsolète.

Il utilise `static_assert` (C11) : le Makefile lui applique `-std=gnu11` via une
règle dédiée, le reste du pak restant en `-std=gnu99`.

## Build & cross-compilation

- `build-desktop.sh` / `build-tg5040.sh` / `build-tg5050.sh` / `run-docker.sh`
- Symlink `.nextui-workspace`
- Toolchain Docker `ghcr.io/loveretro/${PLATFORM}-toolchain`
- `CROSS_COMPILE=aarch64-nextui-linux-gnu-`

`lib/build-libsmb2.sh` compile libsmb2 en statique par plateforme avec
`-DENABLE_LIBKRB5=OFF -DENABLE_GSSAPI=OFF` : laissé à l'auto-détection, Kerberos
reste désactivé dans les toolchains des consoles (pas de krb5 dans leur sysroot)
mais s'active en build natif sous macOS, où l'édition de liens échoue ensuite
contre le GSS système.

## Stratégie de test

- `build-desktop.sh` comme outil principal d'itération.
- Conteneur Samba Docker jetable pour tester le SMB réel, ex. :
  `docker run -d -p 1445:445 -v $(pwd)/share:/share dperson/samba -u "tester;secret" -s "Roms;/share;yes;no;no;tester" -p`
- Pas de tests automatisés dans le dépôt (comme gift-code). Les modules de
  synchro étant sans dépendance à SDL, ils se pilotent en revanche depuis un
  petit harnais en ligne de commande (stubs pour `defines.h`/`api.h`/`utils.h`),
  ce qui couvre vérification, synchro, erreurs, Miroir et annulation sans UI.
- Validation manuelle sur device via `build-tg5040.sh`/`build-tg5050.sh` + ADB
  avant chaque release.

## Risques techniques / points à valider

- **Vérification bloquante** : `sync_engine_check()` est synchrone. L'UI se
  redessine entre deux liaisons mais gèle pendant chacune, et B n'interrompt
  qu'entre deux liaisons. À réévaluer sur un partage volumineux ; un thread
  dédié serait la solution.
- **Détection « déjà présent » par nom + taille** : un fichier distant modifié
  sans changement de taille n'est pas recopié.
- **Casse des noms de fichiers** : exFAT/FAT32 est insensible à la casse, pas la
  comparaison faite ici — un renommage distant limité à la casse peut donner un
  diff incohérent.
- **Noms exacts des variables d'environnement** (`$SHARED_USERDATA_PATH`,
  `$SDCARD_PATH`) supposés stables (utilisés tels quels par gift-code) mais pas
  encore confirmés sur un vrai environnement NextUI.

## Validation : cross-compilation de libsmb2 (2026-09-15)

Testé indépendamment de l'app, en pointant `cmake` directement sur le
compilateur du conteneur `ghcr.io/loveretro/tg5040-toolchain` :

- Toolchain confirmée dans l'image : `aarch64-nextui-linux-gnu-gcc`
  (crosstool-NG 1.25.0, gcc 8.3.0), `cmake` 3.28.3, `make`, `git`.
- Configure CMake avec un toolchain file minimal, `-DBUILD_SHARED_LIBS=OFF
  -DENABLE_EXAMPLES=OFF -DENABLE_LIBDCERPC=OFF` : réussi sans intervention.
- `make` : produit `libsmb2.a` (~630KB, ELF `aarch64`).
- Test de link avec `smb2_init_context()`/`smb2_destroy_context()` : réussi.

Le pak complet (libsmb2 + tomlc17 + sources) se cross-compile depuis, vérifié
sur `tg5040`.
