# Espgotchi 🥚✨  

Port **ESP32 CYD (Cheap Yellow Display)** d’**ArduinoGotchi** (émulation Tamagotchi P1 via TamaLIB), avec :

- **UI tactile**
- **rendu TFT couleur**
- **gestion du temps (SPD)**
- **audio via LEDC**
- une architecture propre basée sur :
  - `VideoService`
  - `InputService`
  - `AudioService`
  - `TamaHost` :contentReference[oaicite:2]{index=2}  

> Objectif : garder le cœur du P1 intact (ROM + TamaLIB), tout en modernisant l’expérience grâce au tactile et à l’écran couleur du CYD.

---

## ✨ Fonctionnalités

- ✅ **Émulation Tamagotchi P1** via **TamaLIB + ROM 12-bit** (héritée d’ArduinoGotchi).
- ✅ **Affichage TFT 320×240** (ILI9341) avec rendu “LCD” agrandi et centré.
- ✅ **Barre d’icônes en haut** (bitmaps du projet original) avec :
  - séparation fine
  - **highlight gris** du slot sélectionné.
- ✅ **3 boutons tactiles visibles en bas** : **L / OK / R**  
  - mapping tactile identique à la logique boutons du core (via `InputService` → `hw_set_button()`).
- ✅ **Injection propre des boutons** dans la CPU via `hw_set_button()`.
- ✅ **Gestion du temps correcte** (fix du `CPU_SPEED_RATIO`) + timer ESP32 fiable.
- ✅ **Bouton vitesse** **SPD x1 / x2 / x4 / x8** en haut à droite :
  - implémenté via **temps virtuel monotone** dans `TamaHost` (pas de freeze lors des changements).
- ✅ **Audio** via sortie **Speaker du CYD** (LEDC, généralement **GPIO 26**) encapsulé dans `AudioService`.
- ✅ Anti-flicker amélioré avec :
  - **limitation FPS d’affichage**,
  - **hash matrice LCD** (skip si inchangé),
  - redraw limité aux zones concernées.

---

## 🧰 Matériel

- ESP32 **Cheap Yellow Display** (ESP32-2432S028R ou équivalent)
- Écran TFT ILI9341 320×240
- Touch XPT2046
- (Optionnel) petit **haut-parleur** branché sur le connecteur **Speaker** du CYD.

---

## 📦 Dépendances

- PlatformIO
- Arduino framework
- `TFT_eSPI`
- `XPT2046_Touchscreen`
- Core ArduinoGotchi/TamaLIB + ROM convertie

---

## 🗂️ Structure

Exemple de structure du dossier `firmware/` :

```text
firmware/
  platformio.ini
  src/
    TamaApp_Headless.cpp      # App principale (instancie les services)
    VideoService.h/.cpp       # Backend vidéo ESP32 CYD (TFT_eSPI + layout)
    InputService.h/.cpp       # Backend input (tactile + hw_set_button)
    AudioService.h/.cpp       # Backend audio (LEDC + speaker)
    TamaHost.h/.cpp           # HAL glue + temps virtuel + boucle TamaLIB

    EspgotchiInput.h/.cpp     # Gestion low-level du touch (XPT2046)
    arduinogotchi_core/
      tamalib.*               # Core TamaLIB
      cpu.*                   # CPU ému
      hw.*                    # Abstraction boutons/LCD/buzzer
      hal.*                   # Interfaces HAL
      rom_12bit.h             # ROM P1 convertie
      bitmaps.h               # Icônes de la topbar
```

> Les anciens fichiers `EspgotchiInputC.*` et `EspgotchiButtons.*` ont été supprimés au profit d’`InputService` qui encapsule input + injection dans `hw_set_button()`. 

---

## 🚀 Build & Flash

### 1) Config PlatformIO (exemple CYD)

```ini
[env:esp32-cyd]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200
upload_speed = 921600
board_build.partitions = no_ota.csv

build_flags =
  -std=c++17
  -D USER_SETUP_LOADED=1

  ; TFT ILI9341 + CYD pins
  -D ILI9341_2_DRIVER=1
  -D TFT_WIDTH=240
  -D TFT_HEIGHT=320
  -D LOAD_GLCD=1
  -D LOAD_FONT2=1
  -D LOAD_FONT4=1
  -D TFT_INVERSION_ON
  -D TFT_RGB_ORDER=TFT_BGR

  -D TFT_MISO=12
  -D TFT_MOSI=13
  -D TFT_SCLK=14
  -D TFT_CS=15
  -D TFT_DC=2
  -D TFT_RST=-1
  -D TFT_BL=21
  -D TFT_BACKLIGHT_ON=HIGH

  ; Touch
  -D USE_HSPI_PORT=1
  -D TOUCH_MOSI=32
  -D TOUCH_MISO=39
  -D TOUCH_SCK=25
  -D TOUCH_CS=33
  -D TOUCH_IRQ=36

lib_deps =
  bodmer/TFT_eSPI @ ^2.5.43
  https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
```

### 2) Compiler

```bash
pio run
```

### 3) Uploader

```bash
pio run -t upload
```

### 4) Serial

```bash
pio device monitor
```

---

## 🧠 Notes importantes

### ROM

La ROM convertie du Tamagotchi P1 doit être disponible dans :

```text
firmware/src/arduinogotchi_core/rom_12bit.h
```

---

## ⌛ Fix critique du timing (déjà intégré)

Le core avait :

```c
#define CPU_SPEED_RATIO 0
```

Ce qui cassait complètement la cadence.

Correctif appliqué :

```c
#ifndef CPU_SPEED_RATIO
#define CPU_SPEED_RATIO 1
#endif
```

---

## 🔊 Audio (via `AudioService`)

ESP32 n’utilise pas `tone()` AVR.
Le son est géré via **LEDC**, piloté par :

```text
TamaLIB -> hw_set_buzzer_freq / hw_enable_buzzer
      -> hal_set_frequency / hal_play_frequency
      -> espgotchi_hal_set_frequency / espgotchi_hal_play_frequency
      -> AudioService::setFrequency / play / stop
      -> LEDC -> Speaker CYD (GPIO 26)
```

`AudioService` offre en plus des helpers `setMuted()` / `setVolume()` pour de futures options UX.

---

## 🧩 Architecture logique

Les quatre services principaux :

* **VideoService** : backend TFT + layout (top bar, LCD, SPD, bottom bar).
* **InputService** : touch XPT2046 + mapping L/OK/R + injection `hw_set_button()`.
* **AudioService** : LEDC buzzer backend.
* **TamaHost** : HAL TamaLIB + temps virtuel + handler() + boucle principale.

Pour le détail complet des flux (input, vidéo, audio, temps, SPD), voir `ARCHITECTURE.md`.

---

## 🧪 État actuel

Cette version est considérée comme **base stable** :

* Émulation OK
* UI tactile OK
* Audio OK (quand un HP est branché)
* Temps OK (SPD x1/x2/x4/x8)
* Architecture découpée en services clairement identifiés.

---

## 🛣️ Roadmap (idées)

* 🎯 **Tactile “smart”** :

  * tap direct sur les icônes (macro d’injection L/OK),
  * swipe pour navigation rapide.
* 🌄 **Background dynamique** :

  * fond jour/nuit selon l’heure.
* 🧹 Extensions sur les services :

  * réglage volume/mute via UI,
  * overlay debug via VideoService.
* 💾 Sauvegarde state (option).
* 🎨 Skins / thèmes.

---

## 🙏 Crédits

* **ArduinoGotchi** par Gary Kwok
* **TamaLIB** par Jean-Christophe Rona
* Communauté CYD / TFT_eSPI / XPT2046

---

## 📜 Licence

Ce projet réutilise des composants sous licence open-source (dont GPL côté core).
Vérifier et respecter les licences d’origine lors de la redistribution.
