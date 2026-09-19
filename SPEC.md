# NextUI Samba Sync — Spécifications (v2)

Pak (type `TOOL`) pour NextUI, permettant de synchroniser (pull uniquement) des dossiers depuis un
partage Samba/SMB du réseau local vers la carte SD de la console.

Inspiré de la structure et du style de [`nextui-gift-code`](https://github.com/dalexanco/nextui-gift-code)
(pak minimaliste en C, pas de dépendances lourdes, UI en state-machine avec les widgets partagés de
NextUI).

## Ce qui change par rapport à la v1

La v1 construisait les jobs sur la console (choix du serveur, navigation distante, navigation
locale). La v2 sépare strictement les deux rôles :

- **Tout ce qui décrit la synchro est dans la config** : serveurs, dossiers distants, dossiers
  locaux cibles, mode. Un seul fichier, édité hors-ligne.
- **L'UI ne fait que déclencher et informer** : elle affiche l'état de chaque liaison, lance la
  synchro, et montre le résultat. Aucune création/édition depuis la console, aucun navigateur de
  fichiers, aucun écran de réglages.

## Portée v2

- **Pull uniquement** : la console télécharge depuis le partage vers la SD. Le contenu distant
  n'est jamais modifié.
- Une ou plusieurs **liaisons** déclarées dans le fichier de config.
- Chaque liaison a un **mode** : *Ajout* (`add`, défaut) ou *Miroir* (`mirror`).
- Vérification automatique à l'ouverture du pak, synchro manuelle de toutes les liaisons en une
  touche. Pas de synchro en arrière-plan.

## Concept

- Une **liaison** = un dossier d'un partage SMB relié à un dossier de la carte SD (ex. :
  `NAS Salon`, partage `Roms`, dossier `GBA` → `Roms/Game Boy Advance (GBA)` sur la SD).
- Un **serveur** = les infos de connexion à une machine Samba (hôte, port, identifiants),
  déclaré une fois et référencé par nom depuis les liaisons.
- Comparaison distant/local par **nom + taille** :
  - absent localement → copié ;
  - présent avec une taille différente → recopié (écrasé) ;
  - présent avec la même taille → ignoré.
- **Mode Ajout** : copie ce qui est nouveau ou différent, ne supprime jamais rien localement.
- **Mode Miroir** : en plus, supprime du dossier local les fichiers et sous-dossiers absents du
  dossier distant. La suppression est strictement limitée au dossier local de la liaison. Pas
  d'écran de confirmation : le nombre de fichiers à supprimer est affiché sur l'écran principal
  après la vérification, avant que l'utilisateur ne lance la synchro.
- **Aucune saisie de texte** sur la console (inchangé depuis la v1).

## Configuration (fichier)

Un fichier unique à la racine de la SD, `Samba Sync.toml`, édité pendant que la carte est montée
sur un ordinateur (ou via SSH). Format : [TOML v1.0](https://toml.io/fr/v1.0.0), lu par
[tomlc99](https://github.com/cktan/tomlc99) (un `.c` + un `.h`, MIT, vendoré dans le dépôt).

**Convention** : toutes les clés, noms de tables et valeurs énumérées sont en **anglais**. Seuls
les noms libres choisis par l'utilisateur (nom de serveur, nom de liaison, chemins) et les textes
affichés par l'UI sont en français.

```toml
[settings]
timeout = 10

[servers."NAS Salon"]
host = "192.168.1.10"
port = 445
username = "guest"
password = ""

[links."Roms GBA"]
server = "NAS Salon"
share = "Roms"
remote = "GBA"
local = "Roms/Game Boy Advance (GBA)"
mode = "mirror"

[links."Roms SNES"]
server = "NAS Salon"
share = "Roms"
remote = "SNES"
local = "Roms/Super Nintendo Entertainment System (SFC)"
```

**Table `[settings]`** — optionnelle.
- `timeout` — entier, délai réseau en secondes, défaut `10`.

**Tables `[servers."<nom>"]`** — une par serveur ; `<nom>` sert d'identifiant pour les liaisons
et est affiché dans l'écran Détail.
- `host` — chaîne, obligatoire, IP ou nom d'hôte.
- `port` — entier, optionnel, défaut `445`.
- `username` / `password` — chaînes, optionnelles, vides ou absentes = accès invité.
- `domain` — chaîne, optionnelle.

**Tables `[links."<nom>"]`** — une par liaison ; `<nom>` est le nom affiché sur la console et
sert de clé pour l'état persistant. L'ordre des tables dans le fichier donne l'ordre d'affichage et
d'exécution.
- `server` — chaîne, obligatoire, nom d'une table `[servers."…"]`.
- `share` — chaîne, obligatoire, nom du partage SMB.
- `remote` — chaîne, optionnelle, chemin dans le partage (vide ou absent = racine du partage).
- `local` — chaîne, obligatoire, chemin relatif à la racine de la SD. Créé s'il n'existe pas.
- `mode` — chaîne, optionnelle : `"add"` (défaut, affiché « Ajout ») ou `"mirror"` (affiché
  « Miroir »).

**Ordre des liaisons** : la norme TOML ne garantit pas l'ordre des clés d'une table. Le pak
s'appuie sur tomlc99, qui les restitue dans l'ordre du fichier. C'est un comportement propre à la
bibliothèque, pas à la norme : un outil externe qui reformate ou trie le fichier peut changer
l'ordre d'affichage. À préciser dans le README.

**Tolérance aux erreurs** :
- Fichier absent → l'écran principal affiche un état vide expliquant où créer
  `Samba Sync.toml`.
- Fichier syntaxiquement invalide (TOML mal formé) → aucune liaison ne peut être lue : l'écran
  principal affiche l'erreur renvoyée par le parseur, avec son numéro de ligne
  ("Samba Sync.toml, ligne 12 : guillemet manquant").
- Serveur mal formé (`host` manquant, `port` non entier) → ignoré ; les liaisons qui le
  référencent apparaissent en erreur "Config : serveur « NAS Salon » invalide".
- Liaison mal formée (champ obligatoire manquant ou du mauvais type, `server` inconnu, `local`
  absolu ou qui sort de la SD via `..`, valeur de `mode` inconnue) →
  **apparaît dans la liste en erreur** avec la raison ("Config : serveur « NAS » introuvable"),
  sans bloquer les autres liaisons. Elle n'est ni vérifiée ni synchronisée.
- Deux serveurs ou deux liaisons portant le même nom → erreur de syntaxe TOML (clé définie deux
  fois) : c'est le cas « fichier invalide » ci-dessus, aucune liaison n'est chargée.
- Clés inconnues ignorées, y compris les tables autres que `settings`, `servers` et `links`
  (permet d'ajouter des options plus tard sans casser les anciennes versions du pak).

**Sécurité** : les mots de passe sont en clair dans le fichier. À signaler dans le README.

**État persistant** : le résultat de la dernière synchro de chaque liaison (date, compteurs,
erreurs) est enregistré par le pak dans `$SHARED_USERDATA_PATH`, indexé par nom de liaison.
Renommer une liaison dans le fichier fait perdre son historique.

---

## Écrans

Deux écrans seulement : la liste des liaisons, et le détail d'une liaison.

### 1. Écran principal — Liste des liaisons

Affiché à l'ouverture du pak. La vérification démarre immédiatement et remplit la liste de façon
progressive, une liaison après l'autre.

Pendant la vérification :

```
Samba Sync
─────────────────────────────────────────
▸ Roms GBA    [Miroir]   ✔ 12 nouveaux · 🗑 3
  Roms SNES              ⟳ Vérification…
  Bios                   · en attente

X  Revérifier   Y  Détail   B  Quitter
```

Vérification terminée :

```
Samba Sync
─────────────────────────────────────────
▸ Roms GBA    [Miroir]   12 nouveaux · 🗑 3
  Roms SNES              À jour
  Bios                   ✘ Serveur injoignable
  Saves                  ✘ Config : share manquant

  Dernière synchro : hier 18:42 · 1 échec

A  Tout synchroniser   X  Revérifier   Y  Détail   B  Quitter
```

**États possibles d'une liaison** (colonne de droite) :
- `· en attente` — pas encore vérifiée.
- `⟳ Vérification…` — connexion et comparaison en cours.
- `N nouveaux` (+ `· 🗑 M` en mode Miroir si des suppressions sont prévues) — des changements sont
  à appliquer.
- `À jour` — rien à copier ni à supprimer.
- `✘ <raison courte>` — la vérification a échoué (réseau, authentification, partage ou dossier
  introuvable) ou la config est invalide.
- Si la **dernière synchro** de cette liaison a échoué (état persistant), un marqueur `!` est
  ajouté devant le nom, même si la vérification courante réussit — le détail est dans l'écran 2.

**Touches** :
- **A** : Tout synchroniser (voir flux ci-dessous). Inactif tant que la vérification n'est pas
  terminée, et si aucune liaison valide n'existe.
- **X** : relance la vérification de toutes les liaisons.
- **Y** : ouvre le détail de la liaison sélectionnée → écran 2.
- **B** : quitte le pak (pendant une vérification : l'interrompt puis quitte).

**Pied d'écran** : date de la dernière synchro (toutes liaisons confondues) et nombre de liaisons
en échec lors de celle-ci.

### Flux « Tout synchroniser »

La synchro se déroule **sur l'écran principal**, sans écran de progression dédié :

```
Samba Sync — Synchronisation 2/4
─────────────────────────────────────────
  Roms GBA    [Miroir]   ✔ 12 copiés · 🗑 3
▸ Roms SNES              ⟳ 5/8 · Chrono Trigger.sfc  62%
  Bios                   · en attente
  Saves                  ✘ Config : share manquant

B  Annuler
```

- Les liaisons valides sont traitées **en séquence**, dans l'ordre du fichier. Les liaisons en
  erreur de config sont sautées. Les liaisons en erreur réseau lors de la vérification sont
  retentées (le réseau a pu revenir).
- Pour chaque liaison : connexion → listing distant → comparaison avec le local (refaite juste
  avant la copie, pour que les suppressions Miroir reposent sur un état à jour) → copie fichier
  par fichier → **en mode Miroir**, suppressions en dernier, seulement si toutes les copies ont
  réussi.
- La ligne de la liaison en cours affiche : fichiers traités / total, nom du fichier courant,
  pourcentage.
- À la fin d'une liaison, sa ligne affiche son résultat : `✔ N copiés · 🗑 M` ou `✘ <raison>`
  (ou `⚠ N copiés · K erreurs` si certains fichiers ont échoué).
- **Un échec n'interrompt pas la file** : l'erreur est enregistrée et la liaison suivante démarre.
- **B** : annule la liaison en cours et toutes les suivantes. Ce qui a été copié reste en place ;
  les suppressions Miroir déjà faites ne sont pas annulées, les restantes ne sont pas exécutées. La
  liaison en cours et les suivantes sont marquées `Annulé`.
- Fin de la file : le titre affiche « Synchronisation terminée », l'état persistant de chaque
  liaison est mis à jour, et les touches normales de l'écran reviennent (A relance, X revérifie,
  Y détail).

### 2. Écran Détail d'une liaison

Accessible via **Y** depuis l'écran principal, en dehors d'une synchro en cours.

```
Roms GBA
─────────────────────────────────────────
Serveur     NAS Salon (192.168.1.10)
Distant     Roms/GBA
Local       Roms/Game Boy Advance (GBA)
Mode        Miroir

Vérification  12 nouveaux (340 Mo) · 3 à supprimer

Dernière synchro  hier 18:42 · ⚠ partielle
  ✔ 84 copiés (1,2 Go)
  🗑 5 supprimés
  ✘ 2 erreurs :
    Pokemon Ruby (USA).gba — espace disque insuffisant
    Zelda Minish Cap (EU).gba — connexion perdue

B  Retour
```

- **Configuration** : rappel en lecture seule de ce que dit le fichier.
- **Vérification** : résultat de la vérification courante (à copier, volume, à supprimer), ou
  l'erreur rencontrée, ou l'erreur de config.
- **Dernière synchro** : date, statut (réussie / partielle / échouée / annulée / jamais), compteurs
  copiés / supprimés, et la liste des erreurs (erreur globale de la liaison, ou erreurs fichier par
  fichier). Liste déroulante si elle dépasse l'écran ; limitée aux 50 premières erreurs.
- **B** : retour à l'écran principal.

---

## Cas d'erreur couverts

Chacun apparaît en raison courte sur l'écran principal, et en clair sur l'écran Détail.

| Cas                                  | Portée              |
|--------------------------------------|---------------------|
| Config invalide (voir tolérance)     | Liaison             |
| Hôte injoignable / timeout           | Liaison             |
| Authentification refusée             | Liaison             |
| Partage ou dossier distant introuvable | Liaison           |
| Dossier local impossible à créer     | Liaison             |
| Espace disque insuffisant            | Fichier (la liaison continue) |
| Connexion perdue en cours de copie   | Liaison (fichiers restants abandonnés) |
| Échec d'écriture/suppression locale  | Fichier             |

Un fichier partiellement téléchargé est supprimé en cas d'échec ou d'annulation (jamais de fichier
tronqué laissé sur la SD).

---

## Features (résumé)

- Config 100 % hors-ligne dans `Samba Sync.toml` (TOML, parseur tomlc99 vendoré) : serveurs,
  liaisons, réglages.
- Écran principal listant les liaisons avec vérification progressive à l'ouverture (nouveaux
  fichiers, suppressions prévues, erreurs).
- « Tout synchroniser » en une touche, exécuté en séquence, avec progression affichée dans la liste.
- Un échec n'arrête pas les autres liaisons ; il est persisté et signalé.
- Écran Détail par liaison : config, vérification, résultat de la dernière synchro, liste des
  erreurs.
- Modes Ajout / Miroir par liaison.

## Hors périmètre v2

- Création/édition des liaisons ou serveurs depuis la console.
- Synchro d'une seule liaison depuis l'UI.
- Push SD → Samba, sync bidirectionnelle.
- Filtres par extension/motif.
- Synchro planifiée / au démarrage / en arrière-plan.
- Comparaison par date de modification ou checksum.
- Reprise d'un fichier interrompu.

## Plateformes ciblées

`tg5040`, `tg5050` (TrimUI Brick / Smart Pro).

## Questions ouvertes

- Emplacement du fichier : racine de la SD (`Samba Sync.toml`) ou à côté du pak
  (`Tools/<plateforme>/Samba Sync.pak/config.toml`) ?
- tomlc99 (TOML v1.0) ou son successeur tomlc17, du même auteur ? À vérifier au moment du
  vendoring (état de maintenance, API, et conservation de l'ordre du fichier, dont dépend l'ordre
  des liaisons).
- Faut-il une synchro d'une seule liaison depuis l'écran Détail (A) ? Exclue pour l'instant.
- Affectation des touches : A = Tout synchroniser / Y = Détail, ou l'inverse (convention NextUI :
  A ouvre l'élément sélectionné) ?
