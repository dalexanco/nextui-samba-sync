# NextUI Samba Sync — Spécifications

Pak (type `TOOL`) pour NextUI, permettant de synchroniser (pull uniquement) des dossiers depuis un
partage Samba/SMB du réseau local vers la carte SD de la console.

Inspiré de la structure et du style de [`nextui-gift-code`](https://github.com/dalexanco/nextui-gift-code)
(pak minimaliste, un seul binaire C, pas de dépendances lourdes, UI en state-machine avec les
widgets partagés de NextUI, réglages persistés en `key=value` dans `$SHARED_USERDATA_PATH`).

## Portée v1

- **Pull uniquement** : la console télécharge depuis le partage Samba vers la SD. Pas de push, pas
  de sync bidirectionnelle (le contenu du partage distant n'est jamais modifié).
- Un ou plusieurs **jobs de sync** configurables, chacun associant un dossier distant (sur un
  partage SMB) à un dossier local sur la carte SD.
- Chaque job a un **mode** : *Ajout simple* (par défaut) ou *Miroir* — voir section Concept.
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
- **Mode Ajout simple** : copie ce qui est nouveau/différent, ne touche jamais aux fichiers déjà
  présents localement qui auraient disparu du partage distant.
- **Mode Miroir** : en plus de la copie, supprime du dossier local les fichiers/sous-dossiers qui
  ne sont plus présents dans le dossier distant du job — le dossier local devient une image exacte
  du dossier distant. La suppression est strictement limitée au dossier de destination du job (rien
  en dehors n'est jamais touché). C'est une opération destructive : voir écran 6 pour le
  comportement de confirmation avant suppression.

---

## Écrans

### 1. Écran d'accueil — Liste des jobs

Liste des jobs de sync configurés, affichés avec un widget liste standard NextUI (pill list) :

```
Samba Sync
─────────────────────────────
▸ Roms GBA  [Miroir]     nas.local/Roms/GBA
  Roms SNES               nas.local/Roms/SNES
  Bios        [Miroir]    nas.local/System/Bios

A  Lancer   X  Ajouter   Y  Éditer   MENU  Réglages   B  Quitter
```

- Chaque entrée montre : nom du job, hôte + chemin distant (tronqué si trop long), un badge de
  mode (`[Miroir]` si activé, rien en mode Ajout simple), et un badge d'état (dernière sync OK /
  jamais synchronisé / dernière sync en erreur).
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
| Mode miroir | Bascule Oui/Non (défaut : Non). Si Oui, la suppression des fichiers locaux absents à distance est activée pour ce job |

- **A** sur un champ texte → ouvre le clavier virtuel.
- **A** sur "Dossier local" → ouvre l'écran 3 (parcourir la SD).
- **A** sur "Mode miroir" → bascule Oui/Non ; en passant à Oui, un texte d'avertissement s'affiche
  inline ("Ce mode supprime les fichiers locaux absents du dossier distant").
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
Synchronisation : Roms GBA (Miroir)
─────────────────────────────
Connexion à nas.local...
Analyse du dossier distant... (128 fichiers)
Comparaison avec le dossier local...

Pokemon Emerald (USA).gba          [███████░░░] 71%
Copié : 84 / 128 fichiers · 340 Mo / 512 Mo
Suppression des fichiers obsolètes : 3 / 5

B  Annuler
```

- Étapes affichées séquentiellement : connexion → listing distant → comparaison avec le contenu
  local → copie fichier par fichier (nom du fichier courant + barre de progression globale, nombre
  de fichiers et volume) → **si mode Miroir**, suppression des fichiers locaux obsolètes en
  dernière étape (après que la copie a réussi, jamais avant).
- Si le réglage "Vérifier avant de synchroniser" est activé, l'étape de comparaison débouche
  d'abord sur l'écran d'aperçu (voir écran 6) avant que la copie/suppression ne démarre réellement.
- **B** : annule proprement la sync en cours (ferme la connexion SMB) → retour écran 1 avec badge
  "Annulé". Ce qui a déjà été copié reste en place ; si l'annulation intervient pendant la phase de
  suppression (mode Miroir), les suppressions déjà effectuées ne sont pas annulées, celles restantes
  ne sont pas exécutées.
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
Synchronisation terminée : Roms GBA (Miroir)
─────────────────────────────
✔ 84 fichiers copiés (340 Mo)
– 44 fichiers déjà présents (ignorés)
🗑 5 fichiers supprimés localement (obsolètes)
✘ 0 erreur

A/B  Retour
```

- La ligne "fichiers supprimés localement" n'apparaît que pour les jobs en mode Miroir.
- Si des erreurs ponctuelles sont survenues sur certains fichiers (copie ou suppression, pas
  bloquantes pour le job entier) : liste déroulante des fichiers concernés avec la raison.
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
- **Vérifier avant de synchroniser** : si Oui, un aperçu est montré avant de lancer la copie
  réelle, avec confirmation explicite (A pour lancer, B pour annuler) :
  - liste des fichiers à copier + volume total ;
  - **pour un job en mode Miroir**, une section séparée liste les fichiers qui seront **supprimés**
    localement, clairement distinguée (ex. préfixe `🗑`) pour que la suppression ne soit jamais une
    surprise.
  - Si ce réglage est sur Non, la sync (copies et éventuelles suppressions en mode Miroir) démarre
    directement sans aperçu ni confirmation intermédiaire.
- **Timeout réseau** : délai avant d'abandonner une connexion qui ne répond pas.

---

## Features (résumé)

- Gestion de plusieurs jobs de sync (CRUD : créer / éditer / supprimer / lister).
- Connexion SMB avec authentification optionnelle (guest ou utilisateur/mot de passe).
- Test de connexion sans copie, depuis l'écran d'édition d'un job.
- Sélection du dossier de destination via navigateur de fichiers natif (pas de saisie manuelle de
  chemin local).
- Sync pull one-way : copie les fichiers présents à distance et absents (ou différents) en local ;
  ne touche jamais au contenu du partage distant.
- Deux modes par job : **Ajout simple** (ne supprime jamais de fichiers locaux) et **Miroir**
  (supprime les fichiers locaux absents du dossier distant, dans la limite du dossier du job).
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
- Filtres par extension/motif, exclusions.
- Sync planifiée / au démarrage / en arrière-plan.
- Plusieurs serveurs Samba partagés entre jobs (actuellement chaque job porte ses propres
  identifiants — pas de "profil serveur" réutilisable).
- Reprise partielle après annulation (reprendre où on s'est arrêté plutôt que tout re-vérifier).

## Plateformes ciblées

Comme `nextui-gift-code` : `tg5040`, `tg5050` (TrimUI Brick / Smart Pro). À confirmer si d'autres
plateformes NextUI sont visées.
