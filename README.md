# Espgotchi 🥚✨  
Port **ESP32 CYD (Cheap Yellow Display)** de **ArduinoGotchi** (émulation Tamagotchi P1 via TamaLIB), avec **UI tactile**, **rendu TFT**, **gestion du temps** et **audio LEDC**.

> Objectif : garder le cœur du P1 intact (ROM + TamaLIB), tout en modernisant l’expérience grâce au tactile et à l’écran couleur du CYD.

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
- ✅ **Bouton vitesse** **SPD x1 / x2 / x4 / x8** en haut à droite.
  - implémentation **temps virtuel monotone** (pas de freeze lors de changements).
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

## 🗂️ Structure

```
firmware/
  platformio.ini
  src/
    AudioService.*         (LEDC, fréquence, mute/volume)
    VideoService.*         (layout TFT, throttle FPS, dirty flags)
    InputService.*         (bridge tactile, held, tap SPD)
    TamaHost.*             (HAL TamaLIB + time scaling)
    TamaApp_Headless.cpp   (composition et cycle de vie)
    EspgotchiInput.*       (tactile + debouncing + zones)
    EspgotchiInputC.*      (bridge C)
    EspgotchiButtons.*     (pump held -> hw_set_button)
    arduinogotchi_core/
      tamalib.*
      cpu.*
      hw.*
      hal.*
      rom_12bit.h
      bitmaps.h
```

> Le nom `TamaApp_Headless.cpp` a été conservé historiquement même si l’app n’est plus “headless”.

L’architecture détaillée des services se trouve dans [`firmware/ARCHITECTURE.md`](firmware/ARCHITECTURE.md).

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
````

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

```
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

```
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

## 🧩 Architecture logique

### 1) Input tactile

* Mapping du touch validé via calibration min/max.
* Zones :

  * bas écran découpé en 3 tiers : LEFT / OK / RIGHT
* Debounce + stable press.

### 2) Bridge C

Pour que le core C reste “propre” :

* `espgotchi_input_*()`
* `espgotchi_buttons_update()`

  * lit `held`
  * appelle `hw_set_button()`.

### 3) UI

* Top bar : icônes menu ArduinoGotchi (render XBM 16×9).
* Bouton vitesse SPD à droite.
* LCD P1 rendu agrandi au centre.
* 3 boutons tactiles visibles en bas.

### 4) Time scaling

* Temps virtuel **monotone** :

  * pas de saut en arrière
  * pas de freeze lors du retour x4 -> x1.

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

* 🎯 **Tactile “smart”** :

  * tap direct sur icônes (macro d’injection L/OK)
  * swipe pour navigation rapide.
* 🌄 **Background dynamique** :

  * fond jour/nuit selon l’heure.
* 🧹 Refacto propre :

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

Ce projet réutilise des composants sous licence open-source (dont GPL côté core).
Vérifier et respecter les licences d’origine lors de la redistribution.

---

Voici un schéma **ASCII/Markdown** que tu peux coller tel quel dans ton README (ou dans un `docs/ARCHITECTURE.md`). J’ai fait en mode “capitalisation maximale” : flux data, modules, dépendances, et points d’extension.

---
## 🧱 Architecture (vue d’ensemble)

```
+---------------------------------------------------------------+
|                         Espgotchi App                          |
|                 (TamaApp_Headless.cpp actuel)                  |
|                                                               |
|  +-------------------+     +-------------------+               |
|  |   Video Layer     |     |   Audio Layer     |               |
|  | (TFT_eSPI render) |     | (LEDC Speaker)    |               |
|  +---------+---------+     +---------+---------+               |
|            |                         |                         |
|            v                         v                         |
|  +-------------------+     +-------------------+               |
|  |  UI Composition   |     |  Audio Backend    |               |
|  | - Top bar icons   |     | - buzzer_init     |               |
|  | - SPD button      |     | - buzzer_play     |               |
|  | - LCD matrix      |     | - buzzer_stop     |               |
|  | - 3 touch buttons |     +-------------------+               |
|  +---------+---------+                                       |
|            |                                                 |
+------------|-------------------------------------------------+
|
v
+---------------------------------------------------------------+
|                         HAL Glue Layer                        |
|                 (implémentation hal_t côté ESP32)             |
|                                                               |
|  halt/log/sleep/get_ts/update_screen/set_matrix/set_icon/...  |
|                                                               |
|  - hal_get_timestamp()  -> temps virtuel monotone + SPD x1/2/4|
|  - hal_sleep_until()    -> cadence réelle                     |
|  - hal_update_screen()  -> Video Layer                        |
|  - hal_set_frequency()  -> current_freq                       |
|  - hal_play_frequency() -> Audio Layer                        |
|  - hal_handler()        -> Input pump + SPD tap               |
+------------------------------+--------------------------------+
|
v
+---------------------------------------------------------------+
|                          TamaLIB Core                         |
|     (porté depuis ArduinoGotchi / TamaLIB + ROM 12-bit)       |
|                                                               |
|  tamalib_init() / tamalib_mainloop_step_by_step()             |
|      |                                                        |
|      | calls g_hal->...                                       |
|      v                                                        |
|  - LCD matrix writes -> hal_set_lcd_matrix()                   |
|  - Icon updates     -> hal_set_lcd_icon()                     |
|  - Buzzer control   -> hal_set_frequency()/play_frequency()   |
|  - Timing requests  -> hal_sleep_until()/get_timestamp()      |
|  - Input check      -> CPU pins via hw_set_button()           |
+------------------------------+--------------------------------+
|
v
+---------------------------------------------------------------+
|                       Hardware Abstraction                     |
|                         (hw.c / cpu.c)                        |
|                                                               |
|  - hw_set_button() -> cpu_set_input_pin(PIN_K00..02)          |
|  - hw_set_lcd_pin() -> g_hal->set_lcd_matrix/icon             |
|  - hw_set_buzzer*() -> g_hal->set_frequency/play              |
|                                                               |
|  ⚠ Fix critique : CPU_SPEED_RATIO non nul                      |
+------------------------------+--------------------------------+
|
v
+---------------------------------------------------------------+
|                          ESP32 CYD HW                          |
|  - TFT ILI9341 320x240   - Touch XPT2046                      |
|  - Speaker (souvent GPIO 26 via JST)                          |
+---------------------------------------------------------------+
```

---

## 🔁 Flux d’input (focus tactile)

```

Touch XPT2046
|
v
EspgotchiInput (C++)

* map raw -> screen coords
* zones bottom bar: LEFT/OK/RIGHT
* debounce + stable press
* held state
* last touch XY (pour UI : SPD etc.)
  |
  v
  EspgotchiInputC (bridge C)
* espgotchi_input_begin/update
* espgotchi_input_peek_held
* espgotchi_input_get_last_touch
  |
  v
  EspgotchiButtons (pump)
* held -> hw_set_button(BTN_*)
  |
  v
  hw.c -> cpu pins -> TamaLIB logic

```

---

## 🖥️ Flux vidéo (LCD P1 -> TFT)

```

TamaLIB CPU
|
| writes segments -> hw_set_lcd_pin()
v
hal_set_lcd_matrix(x,y,val)
|
v
matrix_buffer[LCD_HEIGHT][LCD_WIDTH/8]
|
| hash + FPS limiter
v
render_matrix_to_tft()
|
v
TFT_eSPI (pixels agrandis)

```

Top bar :

```

icon_buffer[] + bitmaps.h
|
v
render_menu_bitmaps_topbar()

* 8 icônes
* highlight slot gris
* séparateur

```

Bottom bar :

```

held state
|
v
render_touch_buttons_bar()

* L / OK / R visibles

```

---

## 🔊 Flux audio (buzzer P1 -> speaker CYD)

```

TamaLIB
|
| hw_set_buzzer_freq(u4)
v
hw_set_buzzer_freq() -> g_hal->set_frequency(freq)
|
v
hal_set_frequency() -> current_freq
|
| hw_enable_buzzer(bool)
v
hal_play_frequency(en)
|
v
buzzer_play/stop (LEDC)
|
v
Speaker CYD (souvent GPIO 26)

```

---

## ⏱️ Flux temps + accélération SPD

```

esp_timer_get_time() (us réels)
|
v
Temps virtuel monotone

* baseRealUs
* baseVirtualUs
* timeMult (1/2/4)
  |
  v
  hal_get_timestamp()
  |
  v
  TamaLIB scheduler
  |
  v
  hal_sleep_until()

```

---

## 🔄 Diagramme de séquence (cycle principal)

```
Utilisateur
|
| 1) touche l'écran
v
Touch XPT2046
|
| 2) lecture SPI
v
EspgotchiInput (C++)
|  - map raw -> coords écran
|  - hit test zones L/OK/R
|  - debounce + stable
|  - held + lastTouchXY
v
EspgotchiInputC (bridge C)
|
| 3) appelé indirectement côté HAL
v
hal_handler()
|
| 4) pump boutons bas
v
EspgotchiButtons
|
| 5) injection CPU pins
v
hw_set_button(BTN_*, state)
|
v
cpu_set_input_pin(PIN_K00..02)
|
v
TamaLIB / CPU emu
|
| 6) un pas d’émulation
v
tamalib_mainloop_step_by_step()
|
| 7) écrit dans l’écran logique
|    (segments / matrix / icônes)
v
hal_set_lcd_matrix(x,y,val)  ---> matrix_buffer[][]
hal_set_lcd_icon(i,val)      ---> icon_buffer[]
|
| 8) update écran (selon framerate interne)
v
hal_update_screen()
|
| 9) throttle FPS réel + hash si activés
v
Video Layer (TFT_eSPI)
|  - render_menu_bitmaps_topbar()
|  - render_speed_button_topbar()
|  - render_matrix_to_tft()
|  - render_touch_buttons_bar()
v
Écran CYD (ILI9341)

```

---

## 🔊 Diagramme de séquence (son)

```
TamaLIB
|
| hw_set_buzzer_freq(u4)
v
hw_set_buzzer_freq()
|
v
hal_set_frequency(freq) -> current_freq
|
| hw_enable_buzzer(true/false)
v
hal_play_frequency(en)
|
v
buzzer_play/stop (LEDC)
|
v
Speaker CYD (GPIO 26 typ.)

```

---

## ⏱️ Diagramme de séquence (temps + SPD)
```

TamaLIB Scheduler
|
| demande timestamp
v
hal_get_timestamp()
|
| temps virtuel monotone:
| baseVirtual + (nowReal-baseReal)*timeMult
v
timestamp (us virtuels)
|
| calcule deadline
v
hal_sleep_until(deadline)
|
| delay/delayMicroseconds
v
Cadence stable

Utilisateur
|
| tap bouton "SPD"
v
hal_handler()
|
| lit lastTouchXY
| set_time_mult(1/2/4)
v
timeMult mis à jour

```

---

## ✅ TL;DR comportement en loop

```

loop():
tamalib_mainloop_step_by_step()
-> hal_handler()       (inputs)
-> hal_get_timestamp() (time)
-> hal_sleep_until()   (cadence)
-> hal_set_lcd_*()     (buffers)
-> hal_update_screen() (TFT)

```

## 🎯 Points d’extension prévus

```

[Future]
+-----------------------------+
| Direct Icon Tap (smart UX)  |
| - tap icône -> macro L/OK   |
| - swipe topbar              |
+-----------------------------+

+-----------------------------+
| Background dynamique        |
| - jour/nuit selon l'heure   |
| - thèmes/skins              |
+-----------------------------+

+-----------------------------+
| Refacto Services            |
| - VideoService              |
| - AudioService              |
| - InputService              |
| - TamaHost                  |
+-----------------------------+

```

---

## ✅ Invariants “golden version”

- Le core TamaLIB/ROM reste inchangé autant que possible.
- Toute modernisation passe par :
  - **HAL**
  - **Input injection**
  - **UI de rendu**
  - **Time virtualization**
- Le tactile **ne doit pas** casser la logique bouton originelle :
  - on simule un humain parfait, pas une nouvelle ROM.

---

## 🧡 TL;DR

Espgotchi est un **Tamagotchi P1 modernisé** :

* même cœur
* meilleure IHM
* tactile
* écran CYD
* vitesse réglable
* son ESP32

Bref : un vrai v-pet rétro… avec un corps du futur.
