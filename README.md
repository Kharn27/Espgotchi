# Espgotchi 🥚✨

Port **ESP32 CYD (Cheap Yellow Display)** de **ArduinoGotchi**  
(émulation Tamagotchi P1 via **TamaLIB**), avec **UI tactile**, **rendu TFT**,  
**gestion du temps** et **audio LEDC**.

> Objectif : garder le cœur du P1 intact (ROM + TamaLIB),  
> tout en modernisant l’expérience grâce au tactile et à l’écran couleur du CYD.

---

## ✨ Fonctionnalités

- ✅ **Émulation Tamagotchi P1** via **TamaLIB + ROM 12-bit** (héritée du projet ArduinoGotchi).
- ✅ **Affichage TFT 320×240** (ILI9341) avec rendu “LCD” agrandi.
- ✅ **Barre d’icônes en haut** (bitmaps du projet original) avec :
  - séparation fine
  - **highlight gris** du slot sélectionné.
- ✅ **3 boutons tactiles visibles en bas** : **L / OK / R**  
  - mapping tactile identique à la logique boutons du core.
- ✅ **Injection propre des boutons** dans la CPU via `hw_set_button()`.
- ✅ **Gestion du temps correcte** (fix du `CPU_SPEED_RATIO`) + timer ESP32 fiable.
- ✅ **Bouton vitesse** **SPD x1 / x2 / x4** en haut à droite  
  - implémentation **temps virtuel monotone** (pas de freeze lors des changements).
- ✅ **Audio** via sortie **Speaker du CYD** (LEDC, généralement **GPIO 26**).
- ✅ Anti-flicker amélioré avec :
  - **limitation FPS d’affichage**
  - **hash matrice LCD** (skip si inchangé)
  - redraw local (pas de full clear global).

---

## 🧰 Matériel

- ESP32 **Cheap Yellow Display** (souvent référencé : ESP32-2432S028R)
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

## 🗂️ Structure (suggestive)

```text
firmware/
  platformio.ini
  src/
    TamaApp_Headless.cpp      (app principale actuelle)
    EspgotchiInput.*          (tactile + debouncing + zones)
    EspgotchiInputC.*         (bridge C)
    EspgotchiButtons.*        (pump held -> hw_set_button)
    arduinogotchi_core/
      tamalib.*
      cpu.*
      hw.*
      hal.*
      rom_12bit.h
      bitmaps.h
```

> Le nom `TamaApp_Headless.cpp` a été conservé historiquement
> même si l’app n’est plus “headless”.

---

## 🚀 Build & Flash

### 1) Config PlatformIO

Exemple de configuration CYD (extrait) :

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

La ROM convertie doit être disponible dans :

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

## 🔊 Audio

ESP32 n’utilise pas `tone()` AVR.
Le son est géré via **LEDC**, piloté par cette chaîne :

```text
hw_set_buzzer_freq -> g_hal->set_frequency
hw_enable_buzzer   -> g_hal->play_frequency
```

Implémentation côté app :

* `buzzer_init()`
* `buzzer_play(freq)`
* `buzzer_stop()`
* `hal_set_frequency()`
* `hal_play_frequency()`

Sortie speaker CYD courante : **GPIO 26**.

---

## 🧪 État actuel

Cette version est considérée comme **base stable** :

* Emulation OK
* UI tactile OK
* Audio OK
* Temps OK
* Quelques micro-glitches visuels possibles lors de navigation très rapide.

---

## 🛣️ Roadmap (idées)

* 🎯 **Tactile “smart”**

  * tap direct sur icônes (macro d’injection L/OK)
  * swipe pour navigation rapide.
* 🌄 **Background dynamique**

  * fond jour/nuit selon l’heure.
* 🧹 Refacto propre

  * `VideoService`, `AudioService`, `InputService`, `TamaHost`.
* 🧪 Mode debug UI toggleable.
* 💾 Sauvegarde state (option).
* 🎨 Skins / thèmes.

---

## 🙏 Crédits

* **ArduinoGotchi** par Gary Kwok
* **TamaLIB** par Jean-Christophe Rona
* Communauté CYD / TFT_eSPI / XPT2046

---

## 📜 Licence

Ce projet réutilise des composants sous licence open-source
(dont GPL côté core). Vérifier et respecter les licences d’origine
lors de la redistribution.
