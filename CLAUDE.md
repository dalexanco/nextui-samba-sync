# nextui-samba-sync

Pak NextUI (type TOOL) qui synchronise des dossiers depuis un partage
Samba/SMB vers la carte SD. v1 : pull uniquement, jobs multiples, mode
Ajout simple ou Miroir au choix par job.

Inspiré de [nextui-gift-code](https://github.com/dalexanco/nextui-gift-code)
dont il reprend la philosophie : C minimal, pas de dépendances superflues,
fichiers de config `key=value`, staging offline dossier+manifest, toolchain
Docker cross-compile. Deux écarts par rapport à ce précédent : plusieurs
fichiers source au lieu d'un seul, et une dépendance vendorée (libsmb2).

## Principe central : aucune saisie de texte libre

Le pak ne propose aucun clavier virtuel. Les serveurs Samba (hôte, partage,
identifiants) sont déclarés hors-ligne dans un fichier de config présent sur
la carte SD (`Samba Servers/<nom>/server.txt`). La création d'un job se fait
entièrement par sélection/navigation sur la console : choix d'un serveur
déclaré → navigation dans l'arborescence distante → navigation dans
l'arborescence locale → toggle mode miroir. Le nom du job est auto-dérivé du
dossier distant choisi.

## Stack technique

- C, SDL2, API partagée NextUI (`GFX_*`, `PAD_*`) via `workspace/all/common/`
- [libsmb2](https://github.com/sahlberg/libsmb2) vendoré en submodule pour le
  client SMB2/3 (statique, Kerberos désactivé, NTLM/guest uniquement)
- Build cross-compile via toolchain Docker `ghcr.io/loveretro/${PLATFORM}-toolchain`,
  plateformes ciblées `tg5040` / `tg5050`

## Pour aller plus loin

- [SPEC.md](SPEC.md) — spec fonctionnelle : écrans, flux, features, hors
  périmètre v1
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — architecture technique
  détaillée : arborescence des sources, modèle de données, modules clés,
  build, gestion des erreurs, stratégie de test, risques techniques
