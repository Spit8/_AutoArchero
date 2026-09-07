# AutoArchero

Application **Qt 6 (C++)** pour visualiser l’écran d’**Archero 2** dans BlueStacks, y appliquer de l’**OCR** et du **template matching**, et envoyer des **taps ADB « fantômes »** (sans souris sur l’émulateur).

L’interface et la capture sont en C++/Qt. L’OCR (RapidOCR) et le matching de templates restent en **Python**, appelés en CLI depuis l’app.

> Application principale : `AutoArchero.exe` (build CMake).  
> L’ancien lanceur PySide (`python -m autoarchero.main`) n’est plus le chemin recommandé.

---

## Sommaire

1. [Fonctionnalités](#fonctionnalités)
2. [Prérequis](#prérequis)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Compilation](#compilation)
6. [Lancement](#lancement)
7. [Utilisation de l’interface](#utilisation-de-linterface)
8. [Modes Test et Gaming](#modes-test-et-gaming)
9. [Zones ROI (OCR)](#zones-roi-ocr)
10. [Templates (boutons / HUD)](#templates-boutons--hud)
11. [CLI OCR (hors UI)](#cli-ocr-hors-ui)
12. [Structure du projet](#structure-du-projet)
13. [Dépannage](#dépannage)

---

## Fonctionnalités

| Fonction | Description |
|---|---|
| Capture ADB | Screenshot PNG via `HD-Adb.exe` / `adb exec-out screencap -p` |
| OCR par zones | Lecture du texte HUD dans des ROI nommées (`assets/rois/*.json`) |
| OCR plein écran | Optionnel : détection hors ROI (hits « ? » en magenta) |
| Templates | Matching OpenCV (`assets/templates/*.png`, seuil 0,82) |
| Live Test | Boucle capture → OCR → pause 1 s → recommence |
| Gaming | OCR à la demande uniquement |
| Tap fantôme | `input tap` au centre de la frame (ou de l’écran ADB) |
| Overlays | Bounding boxes OCR (vert / magenta) et templates (orange) |

---

## Prérequis

### Logiciels

- **Windows** (chemins et scripts fournis pour ce système)
- **Qt 6.11.x** MinGW (ex. `C:\Qt\6.11.2\mingw_64`)
- Outils Qt associés : **MinGW 13.1**, **Ninja**, **CMake** ≥ 3.21
- **BlueStacks** (NXT recommandé) avec ADB activé  
  Binaire typique : `C:\Program Files\BlueStacks_nxt\HD-Adb.exe`
- **Python 3.10+** (venv local `.venv`)

### Dépendances Python

Listées dans `requirements.txt` :

- `opencv-python`, `numpy`, `Pillow`
- `rapidocr-onnxruntime` (moteur OCR)
- `PySide6` (ancien UI Python, optionnel pour le build C++)

---

## Installation

```powershell
git clone <url-du-depot> AutoArchero
cd AutoArchero

python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Copier la config d’exemple puis adapter les chemins :

```powershell
Copy-Item config.ini.example config.ini
```

---

## Configuration

Fichier **`config.ini`** à la racine (ignoré par Git — ne pas le committer) :

```ini
[paths]
project_root=
python_exe=
adb_path=
```

| Clé | Rôle | Défaut si vide |
|---|---|---|
| `project_root` | Racine du dépôt (assets, src) | Dossier source compilé dans le binaire (`AUTOARCHERO_SOURCE_DIR`) |
| `python_exe` | Interpréteur Python du venv | `<project_root>/.venv/Scripts/python.exe` |
| `adb_path` | Exécutable ADB BlueStacks | `C:/Program Files/BlueStacks_nxt/HD-Adb.exe` |

Exemple :

```ini
[paths]
project_root=C:/dev/AutoArchero
python_exe=C:/dev/AutoArchero/.venv/Scripts/python.exe
adb_path=C:/Program Files/BlueStacks_nxt/HD-Adb.exe
```

Avant de lancer l’app :

1. Démarrer **BlueStacks** et Archero 2
2. Vérifier qu’un device ADB apparaît :

```powershell
& "C:\Program Files\BlueStacks_nxt\HD-Adb.exe" devices
```

Un appareil `device` (souvent `emulator-5554`) doit être listé.

---

## Compilation

### Script fourni

```powershell
.\build-mingw.bat
```

Ce script configure le `PATH` (MinGW, Ninja, Qt), génère le build Ninja dans `build/` et compile.

Si Qt n’est pas dans `C:\Qt\6.11.2\mingw_64`, éditez les variables `QT` / `MINGW` / `NINJA` dans `build-mingw.bat`.

### À la main

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.11.2\mingw_64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Qt Creator

1. Ouvrir `CMakeLists.txt`
2. Kit **MinGW 13.1** + Qt **6.11.x**
3. `CMAKE_PREFIX_PATH` = chemin de votre kit Qt (ex. `C:/Qt/6.11.2/mingw_64`)
4. Build & Run (Debug)

---

## Lancement

```powershell
$env:PATH = "C:\Qt\6.11.2\mingw_64\bin;" + $env:PATH
.\build\AutoArchero.exe
```

Sous Qt Creator, le kit ajoute déjà les DLL Qt au PATH.

Au démarrage, l’app charge :

- les ROI depuis `assets/rois/`
- les templates depuis `assets/templates/`
- `config.ini` pour Python / ADB / racine projet

---

## Utilisation de l’interface

Fenêtre **AutoArchero — OCR Visu** :

- **À gauche** : visionneuse de la dernière capture (zoom / pan)
- **À droite** : trois onglets — **Capture**, **Zones**, **Templates**

### Navigation image

| Action | Effet |
|---|---|
| Molette | Zoom |
| Bouton du milieu + glisser | Pan |
| `+` / `−` / `Fit` | Zoom in / out / ajuster à la vue |

### Onglet Capture

| Contrôle | Rôle |
|---|---|
| **Mode** | `Test` ou `Gaming` |
| **OCR plein écran** | Ajoute l’OCR hors ROI (plus lent, utile pour découvrir du texte) |
| **Start live (2s)** | Démarre la boucle live (mode Test) |
| **Stop live** | Arrête la boucle |
| Indicateur LIVE / PAUSE | Vert = live actif, rouge = arrêté |
| **OCR now (Gaming)** | Une capture + OCR immédiate |
| **Status** + **Debug** | Derniers événements ; **Copier debug** pour le presse-papiers |
| **Tap fantôme (centre)** | Tap ADB au centre de la frame |
| **Hits** | Liste `Nom: Valeur` (OCR) et templates matchés |

Couleurs des hits :

- **Vert** — OCR lié à une ROI nommée
- **Magenta** — OCR plein écran sans nom (`?`)
- **Orange** — match de template

### Onglet Zones

Définition des régions d’intérêt pour l’OCR ciblé.

1. Cocher **Dessiner ROI**
2. Tracer un rectangle sur l’image
3. Donner un nom (ex. `or`, `energie`, `pseudo`)
4. **Sauver box → ROI** → écrit `assets/rois/<nom>.json`
5. **Supprimer ROI** (bouton ou clic droit sur la liste)
6. **Recharger depuis disque** si les JSON ont été édités à la main
7. Option **Afficher guides ROI (cyan)** pour voir les zones enregistrées

**Astuce gaming** : clic droit sur une bounding box OCR → **Renommer** → crée / met à jour une ROI autour de la box (avec un léger padding).

### Onglet Templates

Pour détecter un bouton ou un élément graphique fixe (pas du texte OCR) :

1. Avoir une frame affichée
2. Dessiner une box (onglet Zones, mode dessin)
3. Nommer le template (ex. `commencer`)
4. **Capturer template** → `assets/templates/<nom>.png` + `.json`

Les matches apparaissent en orange sur l’image et dans la liste Hits.

---

## Modes Test et Gaming

### Mode Test

- Boutons **Start live** / **Stop live** actifs
- À chaque cycle : capture ADB → OCR (+ templates) → affichage
- Après la fin d’un cycle, **1 seconde** d’attente puis nouvelle capture
- Idéal pour calibrer les ROI et vérifier la stabilité de l’OCR

### Mode Gaming

- Pas de boucle automatique
- **OCR now** ou appel programmatique `requestFrameOcr(reason)` pour une analyse ponctuelle
- Moins de charge CPU / ADB pendant le jeu
- Le label « Dernière demande » indique la raison (`manual`, `automate`, …)

Passer en Gaming arrête automatiquement le live Test.

---

## Zones ROI (OCR)

Chaque fichier `assets/rois/<nom>.json` :

```json
{
  "name": "or",
  "x": 100,
  "y": 20,
  "w": 120,
  "h": 40,
  "enabled": true
}
```

- Coordonnées en pixels **frame native** (résolution screencap BlueStacks)
- L’OCR lit le texte **dans** la box ; le pipeline garde le hit le plus riche par ROI
- ROI fournies en exemple : `or`, `gemmes`, `energie`, `puissance`, `pseudo`, capacités, etc.

Si la résolution BlueStacks change, **redessinez** les ROI (les anciennes boxes ne seront plus alignées).

---

## Templates (boutons / HUD)

Paire de fichiers dans `assets/templates/` :

- `<nom>.png` — crop de référence
- `<nom>.json` — métadonnées (position à la capture, taille frame, date)

Matching OpenCV `TM_CCOEFF_NORMED`, seuil **0,82**. Un template trop petit / trop grand par rapport à la frame est ignoré.

---

## CLI OCR (hors UI)

Utile pour tester sans lancer l’exe :

```powershell
.\.venv\Scripts\Activate.ps1
$env:PYTHONPATH = "$PWD\src"
python -m autoarchero.ocr_cli --image chemin\vers\frame.png --rois assets\rois --templates assets\templates
```

Options :

| Flag | Effet |
|---|---|
| `--image` | PNG à analyser (obligatoire) |
| `--rois` | Dossier des ROI JSON |
| `--templates` | Dossier des templates (optionnel) |
| `--full` | OCR plein cadre en plus des ROI |

Sortie : JSON UTF-8 sur stdout (`ocr`, `templates`, éventuellement `warning`).  
En cas d’erreur, un détail peut être écrit dans `outputs/ocr_cli_last_error.txt` (dossier ignoré par Git).

---

## Structure du projet

```
AutoArchero/
├── app/                 # Sources C++ (MainWindow, AdbClient, OcrBridge, …)
├── ui/mainwindow.ui     # Interface Qt Designer
├── src/autoarchero/     # Package Python (OCR CLI, moteur, templates)
├── assets/
│   ├── rois/            # Zones OCR (JSON)
│   └── templates/       # Crops PNG + JSON
├── build/               # Artefacts CMake (ignoré)
├── outputs/             # Logs d’erreur OCR (ignoré)
├── .venv/               # Environnement Python (ignoré)
├── config.ini           # Chemins locaux (ignoré)
├── config.ini.example
├── CMakeLists.txt
├── build-mingw.bat
├── requirements.txt
└── README.md
```

Flux runtime :

```
BlueStacks ──ADB screencap──► CaptureWorker
                                  │
                                  ▼
                           PNG temporaire
                                  │
                                  ▼
              python -m autoarchero.ocr_cli  (RapidOCR + OpenCV)
                                  │
                                  ▼
                     JSON → overlays + liste Hits
```

---

## Dépannage

| Symptôme | Piste |
|---|---|
| `No ADB device online` | BlueStacks ouvert ? `HD-Adb.exe devices` liste-t-il un `device` ? |
| `cannot start adb` | Vérifier `adb_path` dans `config.ini` |
| `cannot start python OCR` | Vérifier `python_exe` et que `.venv` a bien `pip install -r requirements.txt` |
| OCR timeout / erreur | Consulter le panneau Debug et `outputs/ocr_cli_last_error.txt` |
| Texte illisible / ROI vides | Recaler les boxes ; activer **OCR plein écran** pour localiser le texte ; zoomer |
| Templates jamais détectés | Recapturer à la **même résolution** ; crop assez distinctif ; seuil 0,82 |
| DLL Qt manquantes au lancement | Ajouter `...\mingw_64\bin` au `PATH` avant d’exécuter l’exe |
| ROI / templates « disparus » | **Recharger depuis disque** ; vérifier `project_root` |

---

## Licence / usage

Projet personnel d’assistance visuelle et d’automatisation locale via ADB. Respectez les conditions d’utilisation d’Archero / Habby et de BlueStacks.
