# Architecture — Espgotchi

Ce document décrit l’architecture technique d’Espgotchi :  
un port **ESP32 CYD** d’**ArduinoGotchi** (émulation Tamagotchi P1) basé sur **TamaLIB + ROM 12-bit**, avec :

- rendu TFT (ILI9341) via **VideoService**
- input tactile (XPT2046) via **InputService**
- boutons virtuels L/OK/R + bouton SPD x1/x2/x4
- audio via LEDC (speaker CYD) via **AudioService**
- glue HAL + temps virtuel via **TamaHost** :contentReference[oaicite:0]{index=0}
- constantes UI partagées (`UiLayout`) et helpers debug (heap/PSRAM)

L’objectif de cette architecture est de **préserver le core d’émulation** et d’apporter les améliorations modernes via une couche de services bien séparés.

**Sources core :**

- `firmware/lib/tamalib/` → sous-module **TamaLIB** (HAL + CPU ArduinoGotchi) ; `hal_types.h` et les headers `hal*.h` gardent leur licence d’origine.
- `firmware/src/arduinogotchi_core/` → wrappers d’intégration ESPGotchi :
  - `espgotchi_tamalib_ext.*` (implémentations HAL/bridge C côté ESP32),
  - `espgotchi_tama_rom.*` + `rom_12bit.h` (ROM P1 convertie depuis ArduinoGotchi),
  - `bitmaps.h` (icônes top bar héritées du projet d’origine).

---

## 1) Principes clés

- **Le cœur Tamagotchi (ROM + TamaLIB) est la référence.**
- Toutes les modernisations passent par :
  - le **HAL** (via `TamaHost`),
  - l’**injection d’input** (via `InputService`),
  - le **rendu UI** (via `VideoService`),
  - la **virtualisation du temps** (SPD) dans `TamaHost`,
  - le **backend audio** (via `AudioService`).
- Le tactile ne doit pas réécrire la logique interne :  
  il **simule un humain** qui appuie sur LEFT/OK/RIGHT.

---

## 2) Vue d’ensemble

```text
+---------------------------------------------------------------+
|                         Espgotchi App                        |
|                     (TamaApp_Headless.cpp)                   |
|                                                               |
|  +-------------------+   +-------------------+   +-----------+|
|  |   VideoService    |   |   AudioService    |   |InputService||
|  | (TFT_eSPI render) |   | (LEDC Speaker)    |   | (Touch +   ||
|  |                   |   |                   |   |  hw_set_   ||
|  +---------+---------+   +---------+---------+   |  button)   ||
|            |                     |              +-----------+ |
+------------|---------------------|---------------------------+
             |                     |
             v                     v
+---------------------------------------------------------------+
|                          TamaHost                             |
|         (implémentation HAL + temps virtuel TamaLIB)          |
|                                                               |
|  - gère:                                                       |
|    * hal_get_timestamp()  -> temps virtuel + SPD x1/2/4/8     |
|    * hal_sleep_until()    -> cadence stable                   |
|    * hal_update_screen()  -> VideoService                     |
|    * hal_set_lcd_matrix/icon()                                |
|    * hal_set_frequency() / hal_play_frequency() -> AudioSvc   |
|    * hal_handler() -> InputService + bouton SPD               |
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
|  - LCD matrix writes -> hal_set_lcd_matrix()                  |
|  - Icon updates     -> hal_set_lcd_icon()                     |
|  - Buzzer control   -> hal_set_frequency()/play_frequency()   |
|  - Timing requests  -> hal_sleep_until()/get_timestamp()      |
|  - Input check      -> CPU pins via hw_set_button()           |
+------------------------------+--------------------------------+
                               |
                               v
+---------------------------------------------------------------+
|                       Hardware Abstraction                    |
|                         (hw.c / cpu.c)                        |
|                                                               |
|  - hw_set_button() -> cpu_set_input_pin(PIN_K00..02)          |
|  - hw_set_lcd_pin() -> g_hal->set_lcd_matrix/icon             |
|  - hw_set_buzzer*() -> g_hal->set_frequency/play              |
|                                                               |
|  ⚠ Fix critique : CPU_SPEED_RATIO non nul                     |
+------------------------------+--------------------------------+
                               |
                               v
+---------------------------------------------------------------+
|                          ESP32 CYD HW                         |
|  - TFT ILI9341 320x240   - Touch XPT2046                      |
|  - Speaker (GPIO 26 via JST)                                  |
+---------------------------------------------------------------+
```

---

## 3) Modules & responsabilités

### 3.1 Input — `InputService`

**`InputService`** encapsule toute la gestion d’input haut niveau :

* s’appuie en interne sur `EspgotchiInput` (C++) pour :

  * calibration raw -> coordonnées écran,
  * zones tactiles bas d’écran : **LEFT / OK / RIGHT**,
  * debouncing + “stable press”,
  * état **held** (0 = NONE, 1 = LEFT, 2 = OK, 3 = RIGHT),
  * dernier touch `(x, y, down)` pour la UI (SPD notamment).
* expose :

  * `update()` → lit le tactile et met à jour `hw_set_button(BTN_*, PRESSED/RELEASED)` pour TamaLIB,
  * détection **tap SPD** (top-right) + **tap debug** (centre écran) → placés dans une file `_tapPending[]`,
  * `getHeld()` → utilisé par le log et par `VideoService` pour la barre de boutons,
  * `consumeTap(LogicalButton::SPEED/DEBUG_CENTER)` → consommé par `TamaHost`.

> Les anciens wrappers C (`EspgotchiInputC`, `EspgotchiButtons`) ont été supprimés :
> ils sont désormais remplacés par `InputService`, plus simple et typé C++.

---

### 3.2 Video — `VideoService`

**`VideoService`** est le backend vidéo **ESP32 CYD + TFT_eSPI** :

* possède son propre `TFT_eSPI` et gère :

  * `initDisplay()` : init driver, rotation, clear, backlight,
  * `begin()` : reset des buffers vidéo internes.
* buffers logiques :

  * `matrix[LCD_HEIGHT][LCD_WIDTH/8]` : écran 48×32 du P1,
  * `icons[ICON_NUM]` : icônes du top bar.
* hooks pour le HAL :

  * `setLcdMatrix(x, y, val)` ← `hal_set_lcd_matrix`,
  * `setLcdIcon(icon, val)`   ← `hal_set_lcd_icon`,
  * `updateScreen()`          ← `hal_update_screen`.
* layout & rendu :

  * **Top bar** :

    * 8 icônes (bitmaps ArduinoGotchi),
    * highlight gris du slot sélectionné,
    * séparateur fin.
  * **Zone LCD** :

    * scaling + centrage dans la zone centrale (constantes `UiLayout`),
    * redraw optimisé via **hash FNV** de la matrice pour éviter les frames identiques,
    * **delta pixel** : comparaison `_matrix` vs `_prevMatrix` pour ne redessiner que les pixels changés après le premier plein rendu.
  * **Bottom bar** :

    * 3 boutons visuels **L / OK / R**,
    * anti-flicker : redraw uniquement si `held` change.
  * **Bouton SPD** :

    * bouton en haut à droite (`SPD x<timeMult>`),
    * méthode utilitaire `isInsideSpeedButton(x,y)` pour `TamaHost`.

---

### 3.3 Audio — `AudioService`

**`AudioService`** encapsule le buzzer/speaker via **LEDC** :

* init :

  * `begin()` configure LEDC sur `BUZZER_PIN` (GPIO 26) et canal dédié.
* API :

  * `setFrequency(freqHz)` : définit la fréquence cible,
  * `play()` / `stop()` : démarre/arrête le son,
  * `setMuted(bool)` + `setVolume(uint8_t 0–255)` (prévu pour futures options UX).
* impl :

  * `ledcWriteTone(BUZZER_CH, freq)` pour la fréquence,
  * `ledcWrite(BUZZER_CH, duty)` pour le volume (mapping 0–255 → 0..1023).
* piloté par `TamaHost` via deux callbacks globaux :

  * `espgotchi_hal_set_frequency(freq)` → `audio.setFrequency(freq)`,
  * `espgotchi_hal_play_frequency(en)`  → `audio.play()` / `audio.stop()`.

---

### 3.4 Temps & HAL — `TamaHost`

**`TamaHost`** est la glue entre TamaLIB et les services :

* gère le **temps virtuel** :

  * `_baseRealUs`, `_baseVirtualUs`, `timeMult`,
  * `getTimestamp()` = `baseVirtual + (nowReal - baseReal) * timeMult`,
  * SPD supporte x1 / x2 / x4 / x8 avec temps **monotone**.
* expose le `hal_t` complet :

  * `get_timestamp` / `sleep_until`,
  * `update_screen`, `set_lcd_matrix`, `set_lcd_icon`,
  * `set_frequency`, `play_frequency`,
  * `handler`.
* `handler()` :

  * appelle `input.update()` → injection boutons CPU,
  * consomme les taps SPD/DEBUG détectés par `InputService`,
  * change `timeMult` en cycle (1 → 2 → 4 → 8 → 1),
  * déclenche `printHeapStats()` sur tap debug au centre,
  * logge les transitions de `held` (LEFT / OK / RIGHT / NONE).
* boucle :

  * `begin(fps, startUs)` → enregistre le HAL dans TamaLIB,
  * `loopOnce()` → `tamalib_mainloop_step_by_step()` + log “alive” toutes les 2 s.

---

## 4) Flux d’input (tactile)

```text
Touch XPT2046
     |
     v
EspgotchiInput (C++)
  - map raw -> screen coords
  - zones bottom: LEFT / OK / RIGHT
  - debounce + stable press
  - held + lastTouch(x,y,down)
  - tap SPD (top-right) / tap debug centre
     |
     v
InputService
  - update() -> hw_set_button(BTN_*, PRESSED/RELEASED)
  - consumeTap(SPEED/DEBUG_CENTER) -> TamaHost (SPD + heap stats)
  - getHeld() -> VideoService (UI bas)
     |
     v
hw.c / cpu.c -> TamaLIB CPU pins
```

---

## 5) Flux vidéo (LCD P1 -> TFT)

```text
TamaLIB CPU
  |
  | writes segments -> hw_set_lcd_pin()
  v
hal_set_lcd_matrix(x,y,val)
  |
  v
VideoService::setLcdMatrix()  -> matrix[][]
  |
  | (hash + FPS limit)
  v
VideoService::updateScreen()
  |
  v
TFT_eSPI:
  - topbar icons
  - SPD button
  - LCD matrix (scaled+centered)
  - bottom L/OK/R buttons
  v
Écran CYD (ILI9341)
```

---

## 6) Flux audio

```text
TamaLIB
  |
  | hw_set_buzzer_freq
  v
hw_set_buzzer_freq()
  |
  v
hal_set_frequency(freq)
  |
  v
espgotchi_hal_set_frequency(freq)
  |
  v
AudioService::setFrequency(freq)

TamaLIB
  |
  | hw_enable_buzzer(true/false)
  v
hal_play_frequency(en)
  |
  v
espgotchi_hal_play_frequency(en)
  |
  v
AudioService::play() / stop()
  |
  v
LEDC -> Speaker CYD (GPIO 26)
```

---

## 7) Temps + SPD

```text
esp_timer_get_time() (us réels)
     |
     v
TamaHost:
  baseRealUs / baseVirtualUs / timeMult
     |
     v
hal_get_timestamp() -> temps virtuel
     |
     v
TamaLIB scheduler
     |
     v
hal_sleep_until() -> delay/delayMicroseconds
```

SPD :

```text
Utilisateur
  |
  | Tap sur zone SPD (top-right)
  v
InputService::getLastTouch()
  |
  v
TamaHost::handleHandler()
  - isInsideSpeedButton(x,y) via VideoService
  - setTimeMult(next)      (1 -> 2 -> 4 -> 8 -> 1)
```

---

## 8) Invariants

* Le core **TamaLIB/ROM** reste intact (hors fix timing `CPU_SPEED_RATIO`).
* Le tactile **simule des boutons physiques** (via `hw_set_button()`).
* SPD ne doit pas :

  * casser la stabilité de l’affichage,
  * provoquer des freeze,
  * casser la logique du scheduler.

---

## 9) Points d’extension

* Tap direct sur une icône → macro L/OK sur le menu.
* Thèmes / backgrounds dynamiques selon l’heure.
* Overlay debug optionnel.
* Slider/paramètres pour le volume et le mute du son.
