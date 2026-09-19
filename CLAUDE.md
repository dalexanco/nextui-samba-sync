# nextui-samba-sync

Pak NextUI (type TOOL) qui synchronise des dossiers depuis un partage
Samba/SMB vers la carte SD. v2 : pull uniquement, plusieurs « liaisons »
déclarées hors-ligne, mode Ajout ou Miroir au choix par liaison.

Inspiré de [nextui-gift-code](https://github.com/dalexanco/nextui-gift-code)
dont il reprend la philosophie : C minimal, pas de dépendances superflues,
config hors-ligne sur la carte SD, toolchain Docker cross-compile. Deux écarts
par rapport à ce précédent : plusieurs fichiers source au lieu d'un seul, et
deux dépendances vendorées (libsmb2, tomlc17).

## Principe central : la config décrit, l'UI déclenche

Le pak ne propose aucune saisie de texte, aucun clavier virtuel et aucune
création depuis la console. Tout ce qui décrit la synchro — serveurs,
identifiants, dossier distant, dossier local, mode — est déclaré dans un
fichier unique sur la carte SD (`Samba Sync.toml`, format TOML), édité depuis
un ordinateur.

Une **liaison** = un dossier d'un partage SMB relié à un dossier de la SD.
L'UI se limite à deux écrans : la liste des liaisons (vérifiées une à une à
l'ouverture, synchro de toutes en une touche, progression affichée dans la
liste) et le détail d'une liaison (config, vérification, dernière synchro et
ses erreurs).

## Stack technique

- C, SDL2, API partagée NextUI (`GFX_*`, `PAD_*`) via `workspace/all/common/`
- [libsmb2](https://github.com/sahlberg/libsmb2) vendoré en submodule pour le
  client SMB2/3 (statique, Kerberos désactivé, NTLM/guest uniquement)
- [tomlc17](https://github.com/cktan/tomlc17) copié dans `src/vendor/tomlc17/`
  pour lire la config (compilé en `-std=gnu11`, le reste du pak en `gnu99`)
- Build cross-compile via toolchain Docker `ghcr.io/loveretro/${PLATFORM}-toolchain`,
  plateformes ciblées `tg5040` / `tg5050`

## Pour aller plus loin

- [SPEC.md](SPEC.md) — spec fonctionnelle : config, écrans, flux, cas
  d'erreur, hors périmètre
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — architecture technique
  détaillée : arborescence des sources, modèle de données, modules clés,
  build, gestion des erreurs, stratégie de test, risques techniques
