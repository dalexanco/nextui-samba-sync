# NextUI Samba Sync — Spécifications

Pak (type `TOOL`) pour NextUI, permettant de synchroniser (pull uniquement) des dossiers depuis un
partage Samba/SMB du réseau local vers la carte SD de la console.

Inspiré de la structure et du style de [`nextui-gift-code`](https://github.com/dalexanco/nextui-gift-code)
(pak minimaliste, un seul binaire C, pas de dépendances lourdes, UI en state-machine avec les
widgets partagés de NextUI, réglages persistés en `key=value` dans `$SHARED_USERDATA_PATH`).

## Portée v1

- **Pull uniquement** : la console télécharge depuis le partage Samba vers la SD. Pas de push, pas
  de sync bidirectionnelle, pas de suppression miroir (ce qui a été supprimé côté serveur reste en
  local).
- Un ou plusieurs **jobs de sync** configurables, chacun associant un dossier distant (sur un
  partage SMB) à un dossier local sur la carte SD.
- Exécution manuelle des jobs (l'utilisateur lance la sync depuis le menu). Pas d'auto-sync en
  arrière-plan/au démarrage en v1 (pourrait être une évolution future).

## Concept

- Un **serveur** = les infos de connexion à une machine Samba (hôte/IP, partage, identifiants).
- Un **job** = un sous-dossier précis du partage à synchroniser vers un dossier précis de la SD
  (ex : `//nas.local/Roms/GBA` → `SDCARD/Roms/Game Boy Advance (GBA)`).
- Le pak ne connaît que des jobs "un dossier distant → un dossier local". Pas de règles
  d'inclusion/exclusion par motif en v1.
- Comportement de copie : les fichiers déjà présents localement (même nom, même taille) sont
  ignorés (skip), les nouveaux fichiers sont copiés. Pas de comparaison par checksum en v1 (juste
  nom + taille, éventuellement date de modif).

---

## Écrans

### 1. Écran d'accueil — Liste des jobs

Liste des jobs de sync configurés, affichés avec un widget liste standard NextUI (pill list) :

```
Samba Sync
─────────────────────────────
▸ Roms GBA         nas.local/Roms/GBA
  Roms SNES        nas.local/Roms/SNES
  Bios             nas.local/System/Bios

A  Lancer   X  Ajouter   Y  Éditer   MENU  Réglages   B  Quitter
```

- Chaque entrée montre : nom du job, hôte + chemin distant (tronqué si trop long), et un badge
  d'état (dernière sync OK / jamais synchronisé / dernière sync en erreur).
- **A** : lance la sync du job sélectionné → écran 4 (Progression).
- **X** : nouveau job → écran 2.
- **Y** : éditer le job sélectionné → écran 2 (pré-rempli).
- **Select/L** (à définir) : supprimer le job sélectionné (avec confirmation).
- **MENU** : Réglages globaux → écran 6.
- Liste vide → message d'état vide invitant à appuyer sur X pour créer un premier job.

### 2. Écran Ajouter/Éditer un job

Formulaire simple, champ par champ, navigation haut/bas + A pour éditer un champ (clavier virtuel
NextUI, même composant que l'écran Wi-Fi pour saisir hôte/identifiants).

Champs :

| Champ | Description |
|---|---|
| Nom du job | Libellé libre affiché dans la liste |
| Hôte / IP | Adresse du serveur Samba (ex. `nas.local` ou `192.168.1.10`) |
| Partage | Nom du partage SMB (ex. `Roms`) |
| Dossier distant | Sous-chemin dans le partage (ex. `GBA/`, vide = racine du partage) |
| Utilisateur | Vide/anonyme par défaut (guest) |
| Mot de passe | Masqué à la saisie |
| Dossier local | Chemin sur la SD, sélectionné via un **navigateur de dossiers** (écran 3), pas saisi à la main |

- **A** sur un champ texte → ouvre le clavier virtuel.
- **A** sur "Dossier local" → ouvre l'écran 3 (parcourir la SD).
- **X** : bouton "Tester la connexion" — tente de lister le dossier distant sans copier de
  fichiers, affiche succès/erreur inline (utile avant de sauvegarder un job foireux).
- **START/A sur "Enregistrer"** : valide et retourne à l'écran 1.
- **B** : annule, retourne à l'écran 1 sans sauvegarder.

### 3. Écran Parcourir la SD (sélection du dossier local)

Navigateur de dossiers minimal, partant de la racine de la SD (ou d'un raccourci `Roms/`), pour
choisir/créer le dossier de destination.

```
Choisir un dossier
─────────────────────────────
📁 Roms/
📁 Bios/
📁 Saves/
─────────────────────────────
X  Nouveau dossier   A  Entrer   Y  Choisir ce dossier
```

- **A** : entre dans le dossier sélectionné.
- **Y** : sélectionne le dossier courant comme destination et revient à l'écran 2.
- **X** : crée un nouveau sous-dossier (saisie du nom via clavier virtuel).
- **B** : remonte d'un niveau (ou annule si à la racine).

### 4. Écran Progression de la sync

Affiché pendant l'exécution d'un job.

```
Synchronisation : Roms GBA
─────────────────────────────
Connexion à nas.local...
Analyse du dossier distant... (128 fichiers)

Pokemon Emerald (USA).gba          [███████░░░] 71%
Copié : 84 / 128 fichiers · 340 Mo / 512 Mo

B  Annuler
```

- Étapes affichées séquentiellement : connexion → listing distant → copie fichier par fichier
  (nom du fichier courant + barre de progression globale, nombre de fichiers et volume).
- **B** : annule proprement la sync en cours (ferme la connexion SMB, ne supprime pas ce qui a déjà
  été copié) → retour écran 1 avec badge "Annulé".
- En cas d'erreur bloquante (perte réseau, auth échouée, partage introuvable) → écran 5.
- À la fin → écran 5bis (résumé).

### 5. Écran Erreur

Affiché quand la sync ne peut pas démarrer ou s'interrompt anormalement.

Cas gérés :
- Hôte injoignable / timeout réseau.
- Authentification refusée.
- Partage ou dossier distant introuvable.
- Dossier local illisible/plein (plus d'espace disque).

```
Erreur de synchronisation
─────────────────────────────
Impossible de se connecter à nas.local (timeout)

Vérifiez que la console est sur le même réseau
que le serveur Samba.

A/B  Retour
```

### 5bis. Écran Résumé (fin de sync, succès ou partiel)

```
Synchronisation terminée : Roms GBA
─────────────────────────────
✔ 84 fichiers copiés (340 Mo)
– 44 fichiers déjà présents (ignorés)
✘ 0 erreur

A/B  Retour
```

- Si des erreurs ponctuelles sont survenues sur certains fichiers (pas bloquantes pour le job
  entier) : liste déroulante des fichiers en erreur avec la raison.
- Met à jour le badge d'état du job affiché à l'écran 1 (date/heure de dernière sync).

### 6. Écran Réglages globaux

```
Réglages
─────────────────────────────
Écraser les fichiers existants     [ Non ]
Vérifier avant de synchroniser     [ Oui ]
Timeout réseau (secondes)          [  10 ]

A  Basculer/éditer   B  Retour
```

- **Écraser les fichiers existants** : Non (défaut, skip si même nom+taille) / Oui (toujours
  retélécharger).
- **Vérifier avant de synchroniser** : si Oui, un aperçu (liste + volume total) est montré avant de
  lancer la copie réelle, avec confirmation.
- **Timeout réseau** : délai avant d'abandonner une connexion qui ne répond pas.

---

## Features (résumé)

- Gestion de plusieurs jobs de sync (CRUD : créer / éditer / supprimer / lister).
- Connexion SMB avec authentification optionnelle (guest ou utilisateur/mot de passe).
- Test de connexion sans copie, depuis l'écran d'édition d'un job.
- Sélection du dossier de destination via navigateur de fichiers natif (pas de saisie manuelle de
  chemin local).
- Sync pull one-way : copie les fichiers présents à distance et absents (ou différents) en local ;
  ne touche jamais au contenu du partage distant ; ne supprime jamais de fichiers locaux.
- Détection "déjà présent" par nom + taille (skip par défaut), avec option pour forcer l'écrasement.
- Progression en temps réel (fichier courant, % global, volume transféré) avec annulation propre.
- Résumé de fin de sync (copiés / ignorés / erreurs) et badge d'état persistant par job dans la
  liste d'accueil.
- Historique minimal : date/heure + statut de la dernière exécution par job (pas de log détaillé
  persistant en v1).
- Réglages globaux : comportement d'écrasement, aperçu avant sync, timeout réseau.
- Configuration des jobs persistée dans `$SHARED_USERDATA_PATH` (fichier texte `key=value`, un
  fichier ou une entrée par job, à préciser en phase technique).

## Hors périmètre v1 (pistes futures)

- Sync bidirectionnelle ou push (SD → Samba).
- Sync miroir avec suppression des fichiers locaux absents à distance.
- Filtres par extension/motif, exclusions.
- Sync planifiée / au démarrage / en arrière-plan.
- Plusieurs serveurs Samba partagés entre jobs (actuellement chaque job porte ses propres
  identifiants — pas de "profil serveur" réutilisable).
- Reprise partielle après annulation (reprendre où on s'est arrêté plutôt que tout re-vérifier).

## Plateformes ciblées

Comme `nextui-gift-code` : `tg5040`, `tg5050` (TrimUI Brick / Smart Pro). À confirmer si d'autres
plateformes NextUI sont visées.
