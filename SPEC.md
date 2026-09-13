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
  Les serveurs sont **déclarés hors-ligne dans un fichier de configuration** sur la carte SD (voir
  section Configuration ci-dessous) — jamais saisis sur la console.
- Un **job** = un sous-dossier précis du partage d'un serveur à synchroniser vers un dossier précis
  de la SD (ex : serveur `NAS Salon`, `Roms/GBA` → `SDCARD/Roms/Game Boy Advance (GBA)`). Les jobs
  sont créés **sur la console**, en choisissant un serveur déjà déclaré puis en naviguant dans son
  arborescence distante et dans la SD — jamais en tapant un chemin ou un nom.
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
- **Principe directeur : aucune saisie de texte libre nulle part dans le pak.** Tout est soit
  déclaré dans un fichier (serveurs), soit choisi par sélection/navigation (jobs). Ce choix évite
  d'avoir à construire ou dépendre d'un clavier virtuel — voir section Configuration pour le détail.

## Configuration des serveurs (fichier)

Comme `Gifts/` dans `nextui-gift-code`, la configuration des serveurs se fait **entièrement
hors-ligne**, en éditant des fichiers pendant que la carte SD est montée sur un ordinateur (ou via
un accès réseau/SSH à la console).

Un sous-dossier par serveur, à la racine de la SD (au même niveau que `Roms/`, `Tools/`, etc.) :

```
Samba Servers/
  NAS Salon/
    server.txt
  NAS Bureau/
    server.txt
```

**`server.txt`** — flat `key=value`, comme `manifest.txt` dans gift-code :

```
name=NAS Salon
host=192.168.1.10
port=445
share=Roms
username=guest
password=
domain=
```

- `name` — obligatoire, nom affiché sur la console (écrans Serveurs, choix du serveur pour un job).
  Indépendant du nom du sous-dossier.
- `host` — obligatoire, IP ou nom d'hôte.
- `share` — obligatoire, nom du partage SMB.
- `port` — optionnel, défaut `445`.
- `username` / `password` — optionnels, vides = accès anonyme/invité.
- `domain` — optionnel, pour les environnements avec contrôleur de domaine.

**Comportement au scan** (au lancement du pak, et sur demande via l'écran Serveurs) :
- Un `server.txt` manquant, ou sans `name`/`host`/`share`, est **ignoré silencieusement** — il
  n'apparaît pas dans la liste, mais ne bloque pas le scan des autres serveurs. Même philosophie
  que le scan de `Gifts/` dans gift-code : les erreurs de config sont non-fatales.
- Un job existant qui référence un serveur disparu ou renommé apparaît en erreur ("Serveur
  introuvable") sans planter le pak — voir écran 5.

**Sécurité** : les mots de passe sont stockés **en clair** dans `server.txt`, comme n'importe quel
fichier texte sur la carte SD (même limite que le mot de passe Wi-Fi de NextUI). À documenter
clairement dans le README pour que l'utilisateur en soit conscient avant d'y mettre des
identifiants sensibles.

---

## Écrans

### 0. Écran d'accueil simplifié (affiché à l'ouverture du pak)

Premier écran vu au lancement du pak — une seule action mise en avant, pas de gestion de détail.

```
Samba Sync
─────────────────────────────

        ▶  Tout synchroniser

  3 jobs configurés · dernière synchro : il y a 2h

A  Tout synchroniser   Y  Gérer les jobs   B  Quitter
```

- **A** : lance la synchronisation de **tous** les jobs configurés, l'un après l'autre (voir "Flux
  Tout synchroniser" ci-dessous) → écran 3bis (Aperçu groupé) si "Vérifier avant de synchroniser"
  est activé, sinon directement écran 4 (Progression, variante multi-jobs).
- **Y** : ouvre l'écran 1 (Gestion des jobs) pour créer/éditer/supprimer ou lancer un job
  individuellement.
- **B** : quitte le pak.
- Le texte sous le bouton résume l'état global : nombre de jobs configurés, date de la dernière
  synchro effectuée (tous jobs confondus) et son statut si en erreur (ex. "dernière synchro : il y
  a 2h · 1 job en erreur").
- Aucun job configuré → "Tout synchroniser" est inactif/grisé, message invitant à appuyer sur Y
  pour créer un premier job.

#### Flux "Tout synchroniser"

- Exécute chaque job configuré, dans l'ordre de la liste, quel que soit son mode (Ajout simple ou
  Miroir) — les jobs Miroir sont inclus, pas de traitement à part.
- Si le réglage "Vérifier avant de synchroniser" est activé : avant de lancer quoi que ce soit,
  l'écran 3bis (Aperçu groupé) calcule et affiche, pour l'ensemble des jobs, le total de fichiers à
  copier/volume, et — mis en avant séparément — la liste cumulée des fichiers qui seraient
  **supprimés** par les jobs en mode Miroir. Une confirmation unique valide l'ensemble ; annuler
  n'exécute rien du tout.
- Écran 4 (Progression), variante multi-jobs : en plus du détail du job en cours (fichier courant,
  %, volume), affiche sa position dans la file (ex. "Job 2/3 : Roms SNES").
- Si un job échoue, l'erreur est mémorisée mais **n'interrompt pas** les jobs suivants de la file ;
  l'échec apparaît dans le résumé final de ce job.
- **B** pendant l'exécution : annule le job en cours **et** les jobs restants de la file (pas
  seulement le job courant) → retour écran 0.
- Écran 5bis (Résumé), variante multi-jobs : un total agrégé en haut, puis le détail
  copiés/ignorés/supprimés/erreurs par job.

### 1. Écran Gestion des jobs — Liste des jobs

Accessible depuis l'écran 0 (touche Y). Liste des jobs de sync configurés, affichés avec un widget
liste standard NextUI (pill list) :

```
Gestion des jobs
─────────────────────────────
▸ Roms GBA  [Miroir]     nas.local/Roms/GBA
  Roms SNES               nas.local/Roms/SNES
  Bios        [Miroir]    nas.local/System/Bios

A  Lancer   X  Ajouter   Y  Éditer   MENU  Réglages   B  Retour
```

- Chaque entrée montre : nom du job, hôte + chemin distant (tronqué si trop long), un badge de
  mode (`[Miroir]` si activé, rien en mode Ajout simple), et un badge d'état (dernière sync OK /
  jamais synchronisé / dernière sync en erreur).
- **A** : lance la sync du job sélectionné uniquement → écran 4 (Progression, job unique).
- **X** : nouveau job → écran 2a (assistant, étape 1/3).
- **Y** : éditer le job sélectionné → écran 2a (assistant, pré-sélectionné sur les choix actuels).
- **Select/L** (à définir) : supprimer le job sélectionné (avec confirmation).
- **MENU** : Réglages globaux → écran 6 (inclut l'accès à l'écran 1bis Serveurs).
- **B** : retour à l'écran 0 (accueil simplifié).
- Liste vide → message d'état vide invitant à appuyer sur X pour créer un premier job. Si en plus
  aucun serveur n'est déclaré, le message invite plutôt à créer un dossier dans `Samba Servers/`.

### 1bis. Écran Serveurs (lecture seule)

Accessible depuis les Réglages (écran 6). Vue de diagnostic sur les serveurs déclarés dans
`Samba Servers/` — aucune création/édition ici, ça se passe en éditant les fichiers sur la SD.

```
Serveurs
─────────────────────────────
▸ NAS Salon      192.168.1.10 / Roms      ✔ connecté
  NAS Bureau     192.168.1.20 / Partage   ✘ injoignable

A  Tester la connexion   X  Recharger   B  Retour
```

- **A** : tente une connexion au serveur sélectionné (sans lister ni copier), affiche le résultat
  inline.
- **X** : recharge la liste depuis `Samba Servers/` — utile si le fichier vient d'être modifié
  pendant que le pak tourne (édition via un pak gestionnaire de fichiers, SSH, etc.).
- Un serveur mal configuré (champ obligatoire manquant) n'apparaît pas dans cette liste (voir règle
  de scan dans la section Configuration) plutôt que d'afficher une entrée cassée.

### 2a. Écran Ajouter/Éditer un job — Choisir un serveur (étape 1/3)

Liste de sélection des serveurs déclarés dans `Samba Servers/` — aucune saisie, juste un choix.

```
Nouveau job — Choisir un serveur
─────────────────────────────
▸ NAS Salon      192.168.1.10 / Roms
  NAS Bureau     192.168.1.20 / Partage

A  Choisir   B  Annuler
```

- **A** : sélectionne le serveur, tente immédiatement une connexion → écran 2b si succès, écran 5
  (erreur) si échec (avec possibilité de revenir choisir un autre serveur).
- **B** : annule l'assistant, retour écran 1.
- Aucun serveur déclaré → message invitant à créer un dossier dans `Samba Servers/` sur la SD,
  bouton A désactivé.

### 2b. Écran Ajouter/Éditer un job — Parcourir le partage distant (étape 2/3)

Navigateur dans l'arborescence SMB du serveur choisi, pour sélectionner le dossier source du job.
Symétrique de l'écran 3 côté distant, alimenté par un listing SMB en direct.

```
NAS Salon — Choisir un dossier distant
─────────────────────────────
📁 Roms/
📁 System/
📁 Saves/
─────────────────────────────
A  Entrer   Y  Choisir ce dossier   B  Retour
```

- **A** : entre dans le dossier distant sélectionné (nouveau listing SMB).
- **Y** : sélectionne le dossier distant courant comme source du job → écran 3.
- **B** : remonte d'un niveau, ou revient à l'écran 2a si à la racine du partage.
- Erreur réseau en cours de navigation (perte de connexion, timeout) → écran 5, avec retour possible
  à l'écran 2a.

### 3. Écran Parcourir la SD (sélection du dossier local, étape 3/3 de l'assistant)

Navigateur de dossiers minimal, partant de la racine de la SD (ou d'un raccourci `Roms/`), pour
choisir l'emplacement du dossier de destination.

```
Choisir un dossier
─────────────────────────────
📁 Roms/
📁 Bios/
📁 Saves/
─────────────────────────────
X  Créer "GBA" ici   A  Entrer   Y  Choisir ce dossier
```

- **A** : entre dans le dossier sélectionné.
- **Y** : sélectionne le dossier courant comme destination → écran 2c (récapitulatif).
- **X** : crée ici un nouveau sous-dossier **nommé automatiquement d'après le dossier distant
  choisi à l'étape 2b** (ex. `GBA`) — pas de saisie de nom. En cas de collision, un suffixe
  numérique est ajouté (`GBA (2)`).
- **B** : remonte d'un niveau (ou annule l'assistant si à la racine).

### 2c. Écran Ajouter/Éditer un job — Récapitulatif (étape 3/3)

Dernière étape : bascule du mode miroir et confirmation, pas de saisie.

```
Nouveau job — Récapitulatif
─────────────────────────────
Nom          GBA
Serveur      NAS Salon
Distant      Roms/GBA
Local        Roms/GBA
Mode miroir  [ Non ]

A  Enregistrer   Y  Basculer le mode miroir   B  Revenir
```

- Le **nom du job** est dérivé automatiquement du nom du dossier distant choisi (non éditable, pas
  de clavier) ; en cas de collision avec un job existant, un suffixe numérique est ajouté.
- **Y** : bascule Oui/Non le mode miroir ; en passant à Oui, un texte d'avertissement s'affiche
  inline ("Ce mode supprime les fichiers locaux absents du dossier distant").
- **A** : enregistre le job et retourne à l'écran 1.
- **B** : revient à l'étape précédente (écran 3) sans enregistrer.

### 3bis. Écran Aperçu (avant sync)

Affiché uniquement si le réglage "Vérifier avant de synchroniser" est activé, juste avant le
démarrage réel d'une sync — que ce soit un job unique (lancé depuis l'écran 1) ou "Tout
synchroniser" (lancé depuis l'écran 0, auquel cas l'aperçu agrège tous les jobs concernés).

```
Aperçu de la synchronisation
─────────────────────────────
À copier : 84 fichiers (340 Mo)

🗑 À supprimer (jobs en mode Miroir) :
  Roms GBA : 5 fichiers
  Bios     : 2 fichiers

A  Lancer la synchronisation   B  Annuler
```

- Job unique : liste/volume à copier pour ce job, et si le job est en mode Miroir, la section
  `🗑 À supprimer` correspondante.
- "Tout synchroniser" : les totaux sont cumulés sur tous les jobs ; la section `🗑 À supprimer` ne
  liste que les jobs en mode Miroir concernés (un sous-total par job), pour que les suppressions
  ne soient jamais noyées dans le volume global à copier.
- **A** : lance réellement la copie (et les suppressions le cas échéant) → écran 4.
- **B** : annule, rien n'est exécuté → retour à l'écran d'origine (0 ou 1).

### 4. Écran Progression de la sync

Affiché pendant l'exécution d'un job (ou d'une file de jobs pour "Tout synchroniser").

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
- Cette étape (écran 4) n'est atteinte qu'après validation de l'écran 3bis (Aperçu) si le réglage
  correspondant est activé ; sinon elle démarre directement au lancement de la sync.
- Variante multi-jobs (déclenchée depuis "Tout synchroniser") : une ligne supplémentaire indique la
  position dans la file (ex. "Job 2/3 : Roms SNES") ; le reste de l'affichage (fichier courant, %,
  volume, suppression) est identique, appliqué au job en cours.
- **B** : annule proprement la sync en cours (ferme la connexion SMB) → retour à l'écran d'origine
  (écran 1 pour un job unique, écran 0 pour "Tout synchroniser") avec badge "Annulé". Ce qui a déjà
  été copié reste en place ; si l'annulation intervient pendant la phase de suppression (mode
  Miroir), les suppressions déjà effectuées ne sont pas annulées, celles restantes ne sont pas
  exécutées. En mode multi-jobs, annuler arrête aussi tous les jobs restants de la file.
- En cas d'erreur bloquante sur un job (perte réseau, auth échouée, partage introuvable) : job
  unique → écran 5 ; multi-jobs → l'erreur est mémorisée et les jobs suivants continuent.
- À la fin → écran 5bis (résumé).

### 5. Écran Erreur

Affiché quand la sync ne peut pas démarrer ou s'interrompt anormalement.

Cas gérés :
- Hôte injoignable / timeout réseau.
- Authentification refusée.
- Partage ou dossier distant introuvable.
- Dossier local illisible/plein (plus d'espace disque).
- Serveur référencé par un job introuvable ou mal configuré (supprimé/renommé dans
  `Samba Servers/` depuis la création du job) — message invitant à vérifier le fichier `server.txt`
  correspondant, ou à éditer le job pour choisir un autre serveur.

```
Erreur de synchronisation
─────────────────────────────
Impossible de se connecter à nas.local (timeout)

Vérifiez que la console est sur le même réseau
que le serveur Samba.

A/B  Retour
```

### 5bis. Écran Résumé (fin de sync, succès ou partiel)

Job unique :

```
Synchronisation terminée : Roms GBA (Miroir)
─────────────────────────────
✔ 84 fichiers copiés (340 Mo)
– 44 fichiers déjà présents (ignorés)
🗑 5 fichiers supprimés localement (obsolètes)
✘ 0 erreur

A/B  Retour
```

Variante multi-jobs ("Tout synchroniser") : total agrégé en haut, puis détail par job.

```
Synchronisation terminée : 3 jobs
─────────────────────────────
✔ 210 fichiers copiés (780 Mo) · 🗑 7 supprimés · ✘ 1 erreur

  Roms GBA  (Miroir)   ✔ 84 copiés · 🗑 5 supprimés
  Roms SNES            ✔ 126 copiés
  Bios      (Miroir)   ✘ erreur (voir détail)

A/B  Retour
```

- La ligne/le badge "fichiers supprimés localement" n'apparaît que pour les jobs en mode Miroir.
- Si des erreurs ponctuelles sont survenues sur certains fichiers (copie ou suppression, pas
  bloquantes pour le job entier), ou si un job entier a échoué (variante multi-jobs) : liste
  déroulante des éléments concernés avec la raison.
- Met à jour le badge d'état de chaque job affiché à l'écran 1 (date/heure de dernière sync), ainsi
  que le résumé global affiché sur l'écran 0.

### 6. Écran Réglages globaux

```
Réglages
─────────────────────────────
Écraser les fichiers existants     [ Non ]
Vérifier avant de synchroniser     [ Oui ]
Timeout réseau (secondes)          [  10 ]
▸ Voir les serveurs...

A  Basculer/éditer   B  Retour
```

- **Voir les serveurs...** : ouvre l'écran 1bis (liste des serveurs déclarés, lecture seule + test
  de connexion).
- **Écraser les fichiers existants** : Non (défaut, skip si même nom+taille) / Oui (toujours
  retélécharger).
- **Vérifier avant de synchroniser** : si Oui, l'écran 3bis (Aperçu) s'affiche avant de lancer la
  copie réelle (job unique ou "Tout synchroniser"), avec confirmation explicite. Si ce réglage est
  sur Non, la sync (copies et éventuelles suppressions en mode Miroir) démarre directement sans
  aperçu ni confirmation intermédiaire.
- **Timeout réseau** : délai avant d'abandonner une connexion qui ne répond pas.

---

## Features (résumé)

- Écran d'accueil simplifié au lancement du pak avec une action "Tout synchroniser" qui exécute
  tous les jobs à la suite (job unique et gestion CRUD relégués à un écran secondaire).
- **Serveurs déclarés hors-ligne** dans `Samba Servers/<nom>/server.txt` sur la carte SD (comme
  `Gifts/` dans gift-code) — jamais saisis sur la console. Scan tolérant : une entrée mal formée est
  ignorée, pas bloquante.
- Écran Serveurs (lecture seule) pour visualiser les serveurs déclarés et tester leur connexion.
- **Aucune saisie de texte libre nulle part** : création d'un job entièrement par sélection/
  navigation (choisir un serveur → parcourir son partage distant → parcourir la SD → basculer le
  mode miroir) ; le nom du job est dérivé automatiquement du dossier distant choisi.
- Gestion de plusieurs jobs de sync (CRUD : créer / éditer / supprimer / lister) référençant chacun
  un serveur déclaré.
- Connexion SMB avec authentification optionnelle (guest ou utilisateur/mot de passe), lue depuis
  la config serveur.
- Test de connexion implicite dès le choix du serveur lors de la création d'un job, et à la demande
  depuis l'écran Serveurs.
- Sélection du dossier de destination via navigateur de fichiers natif, avec création automatique
  du dossier nommé d'après le dossier distant choisi (pas de saisie manuelle de nom).
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
- Configuration des jobs (référence au serveur par nom, chemins distant/local, mode miroir)
  persistée par le pak dans `$SHARED_USERDATA_PATH` (fichier texte `key=value`, un fichier par job,
  à préciser en phase technique) — distincte de `Samba Servers/`, qui reste éditable par
  l'utilisateur.

## Hors périmètre v1 (pistes futures)

- Sync bidirectionnelle ou push (SD → Samba).
- Filtres par extension/motif, exclusions.
- Sync planifiée / au démarrage / en arrière-plan.
- Édition/suppression des serveurs depuis la console (se fait en éditant `Samba Servers/` sur un
  ordinateur ou via SSH).
- Renommage manuel du nom d'un job après création (dérivé automatiquement, non éditable en v1).
- Reprise partielle après annulation (reprendre où on s'est arrêté plutôt que tout re-vérifier).

## Plateformes ciblées

Comme `nextui-gift-code` : `tg5040`, `tg5050` (TrimUI Brick / Smart Pro). À confirmer si d'autres
plateformes NextUI sont visées.
