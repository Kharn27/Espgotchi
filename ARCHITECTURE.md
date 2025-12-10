# Architecture — Espgotchi

Ce document décrit l’architecture technique d’Espgotchi :  
un port **ESP32 CYD** d’**ArduinoGotchi** (émulation Tamagotchi P1) basé sur **TamaLIB + ROM 12-bit**, avec :

- rendu TFT (ILI9341)
- input tactile (XPT2046)
- boutons virtuels L/OK/R
- bouton vitesse SPD x1/x2/x4
- audio via LEDC (sortie speaker CYD)

L’objectif de cette architecture est de **préserver le core d’émulation** et d’apporter les améliorations modernes via une couche HAL/UX propre.

---

## 1) Principes clés

- **Le cœur Tamagotchi (ROM + TamaLIB) est la référence.**
- Toutes les modernisations passent par :
  - le **HAL**,
  - l’**injection d’input**,
  - le **rendu UI**,
  - la **virtualisation du temps**.
- Le tactile ne doit pas réécrire la logique interne :  
  il **simule un humain** qui appuie sur LEFT/OK/RIGHT.

---

## 2) Vue d’ensemble

```text
+---------------------------------------------------------------+
|                         Espgotchi App                          |
|              (TamaApp_Headless.cpp / app principale)           |
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
|                (implémentation hal_t côté ESP32)              |
|                                                               |
|  halt/log/sleep/get_ts/update_screen/set_matrix/set_icon/...  |
|                                                               |
|  - hal_get_timestamp()  -> temps virtuel monotone + SPD x1/2/4|
|  - hal_sleep_until()    -> cadence stable                     |
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
|  ⚠ Fix critique : CPU_SPEED_RATIO non nul                     |
+------------------------------+--------------------------------+
                               |
                               v
+---------------------------------------------------------------+
|                          ESP32 CYD HW                          |
|  - TFT ILI9341 320x240   - Touch XPT2046                      |
|  - Speaker (souvent GPIO 26 via JST)                          |
+---------------------------------------------------------------+
````

---

## 3) Modules & responsabilités

### 3.1 Input

* **EspgotchiInput (C++)**

  * calibration raw -> écran
  * zones tactiles bas d’écran : LEFT / OK / RIGHT
  * debouncing + “stable press”
  * état `held`
  * dernier touch `(x,y,down)` pour UI additionnelle (SPD)

* **EspgotchiInputC (bridge C)**

  * expose un API C minimal au reste de l’app / HAL

* **EspgotchiButtons**

  * lit `held`
  * injecte l’input dans TamaLIB via :

    * `hw_set_button(BTN_*, PRESSED/RELEASED)`

### 3.2 Video

* Buffer LCD P1 :

  * `matrix_buffer[LCD_HEIGHT][LCD_WIDTH/8]`
  * `icon_buffer[ICON_NUM]`

* Rendu TFT composé de :

  * top bar icônes (bitmaps ArduinoGotchi)
  * bouton SPD dans la top bar
  * matrice LCD agrandie et centrée
  * barre boutons tactiles visibles en bas

* Anti-flicker :

  * limitation FPS d’affichage (temps réel)
  * hash matrix (skip si inchangé)

### 3.3 Audio

* LEDC backend :

  * `buzzer_init()`
  * `buzzer_play(freq)`
  * `buzzer_stop()`

* Chaîne d’appel :

  * `hw_set_buzzer_freq()` -> `hal_set_frequency()`
  * `hw_enable_buzzer()`   -> `hal_play_frequency()`

### 3.4 Time

* Temps réel :

  * `esp_timer_get_time()` en microsecondes

* Temps virtuel **monotone** :

  * évite les sauts en arrière
  * évite les freeze lors de `x4 -> x1`

---

## 4) Flux d’input (tactile)

```text
Touch XPT2046
     |
     v
EspgotchiInput (C++)
- map raw -> screen coords
- hit test zones L/OK/R
- debounce + stable press
- held state
- last touch XY (pour UI : SPD)
     |
     v
EspgotchiInputC (bridge C)
- espgotchi_input_begin/update
- espgotchi_input_peek_held
- espgotchi_input_get_last_touch
     |
     v
EspgotchiButtons (pump)
- held -> hw_set_button(BTN_*)
     |
     v
hw.c -> cpu pins -> TamaLIB logic
```

---

## 5) Flux vidéo (LCD P1 -> TFT)

```text
TamaLIB CPU
  |
  | writes segments -> hw_set_lcd_pin()
  v
hal_set_lcd_matrix(x,y,val)  ---> matrix_buffer[][]
hal_set_lcd_icon(i,val)      ---> icon_buffer[]
  |
  | FPS limiter + hash
  v
hal_update_screen()
  |
  v
TFT_eSPI
- render_menu_bitmaps_topbar()
- render_speed_button_topbar()
- render_matrix_to_tft()
- render_touch_buttons_bar()
```

---

## 6) Flux audio

```text
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
Speaker CYD (souvent GPIO 26)
```

---

## 7) Flux temps + SPD

```text
esp_timer_get_time() (us réels)
     |
     v
Temps virtuel monotone
- baseRealUs
- baseVirtualUs
- timeMult (1/2/4)
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

## 8) Diagrammes de séquence

### 8.1 Cycle principal

```text
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
EspgotchiInputC
   |
   v
hal_handler()
   |
   | 3) pump boutons bas
   v
EspgotchiButtons
   |
   | 4) injection CPU pins
   v
hw_set_button(BTN_*, state)
   |
   v
cpu_set_input_pin(PIN_K00..02)
   |
   v
TamaLIB / CPU emu
   |
   | 5) un pas d’émulation
   v
tamalib_mainloop_step_by_step()
   |
   | 6) writes LCD & icons
   v
hal_set_lcd_matrix/icon()
   |
   | 7) update écran
   v
hal_update_screen()
   |
   | 8) throttle + hash
   v
TFT_eSPI render
   |
   v
Écran CYD
```

### 8.2 Son

```text
TamaLIB
  |
  | hw_set_buzzer_freq / hw_enable_buzzer
  v
hw.c
  |
  v
hal_set_frequency -> current_freq
hal_play_frequency(en)
  |
  v
LEDC buzzer backend
  |
  v
Speaker CYD
```

### 8.3 SPD

```text
Utilisateur
  |
  | tap sur "SPD"
  v
hal_handler()
  |
  | détecte zone top-right
  | set_time_mult()
  v
timeMult mis à jour
  |
  v
hal_get_timestamp() (temps virtuel monotone)
```

---

## 9) Invariants de la “golden version”

* La logique interne du P1 est non modifiée (sauf fix timing nécessaire).
* L’UX tactile actuelle est strictement équivalente aux 3 boutons originaux.
* L’accélération SPD ne doit pas :

  * dégrader la stabilité d’affichage,
  * provoquer de freeze,
  * casser la logique du scheduler.

---

## 10) Points d’extension

```text
[Future]
- Direct Icon Tap :
  tap icône -> macro L(L...) + OK
- Background dynamique :
  jour/nuit selon heure
- Refacto Services :
  VideoService / AudioService / InputService / TamaHost
- Mode debug overlay
- Skins & thèmes
```
