# Architecture firmware Espgotchi

Ce document fusionne l’ancienne description d’architecture avec la nouvelle séparation en services. Il sert de boussole rapide pour comprendre les couches logicielles, le flux d’exécution et les points d’extension de l’application.

## Couches logicielles

- **TamaLIB + ROM 12-bit** : cœur d’émulation hérité d’ArduinoGotchi, non modifié.
- **HAL Espgotchi** : implémente les hooks `hal_*` attendus par TamaLIB (LCD, icônes, audio, temps, boutons).
- **Services applicatifs** : encapsulent les périphériques du CYD (tactile, TFT, haut-parleur) et la logique de cadence.
- **Entrée Arduino** (`setup()/loop()`) : compose les services, effectue le câblage HAL et délègue le cycle de vie à `TamaHost`.

## Services

### AudioService
- Initialisation LEDC (canal + pin haut-parleur CYD).
- Gestion de la fréquence courante, lecture/arrêt.
- Mute et contrôle du duty-cycle (volume symbolique).

### VideoService
- Mise en page TFT : topbar icônes, matrice LCD, barre tactile.
- Flags de salissure + throttling FPS (12 FPS).
- Calcul de hash sur la matrice pour éviter les redraws inutiles.
- Bouton vitesse (SPD xN) piloté par un provider `timeMult`.

### InputService
- Bridge tactile existant (`EspgotchiInputC`) + mapping L/OK/R.
- Détection du dernier touch XY + edge «tap» pour la zone SPD.
- Injection des états maintenus dans TamaLIB via `espgotchi_buttons_update()`.

### TamaHost
- Glue HAL TamaLIB (timestamp, sleep, LCD, icônes, audio, handler).
- Virtualisation du temps avec multiplicateur monotone.
- Gestion des logs d’inputs et délégation écran/audio aux services.

## Flux d’exécution

1. `setup()` instancie les services, lance un bip de vérification puis enregistre le HAL via `TamaHost::begin()`.
2. `loop()` appelle `TamaHost::loopStep()` qui exécute `tamalib_mainloop_step_by_step()`.
3. TamaLIB déclenche les callbacks HAL :
   - `hal_handler()` → `InputService::poll()` (touch + mapping boutons) + détection du bouton SPD.
   - `hal_update_screen()` → `VideoService::update()` (throttle + redraws ciblés).
   - `hal_set_lcd_*()` → marquage des buffers comme «dirty» pour le rendu.
   - `hal_set_frequency()` / `hal_play_frequency()` → `AudioService`.

## Rendu TFT

- Les buffers `matrix_buffer` (LCD 128×32 bits) et `icon_buffer` (barre d’icônes) sont gérés en RAM par l’app.
- `VideoService::setLcd*` marque ces buffers «dirty». `VideoService::update()` déclenche le redraw si le throttle FPS le permet et si le hash LCD a changé.
- Les boutons tactiles L / OK / R sont dessinés dans une barre basse dédiée ; le bouton SPD occupe le coin supérieur droit.

## Entrées tactiles

- `InputService` s’appuie sur `EspgotchiInput` (debounce + calibration) exposé en C via `EspgotchiInputC`.
- Mapping direct vers TamaLIB : L → `BUTTON_LEFT`, OK → `BUTTON_MIDDLE`, R → `BUTTON_RIGHT`.
- Un tap court dans la zone SPD déclenche un edge pour le changement de multiplicateur de temps.

## Audio

- `hal_set_frequency()` enregistre la note courante via `AudioService::setFrequency()`.
- `hal_play_frequency()` déclenche `AudioService::play()` ou `stop()` selon l’état de buzzer demandé par TamaLIB.
- Le volume reste symbolique (duty-cycle LEDC) pour éviter la saturation du haut-parleur CYD.

## Cadence & temps virtuel

- `TamaHost` maintient un multiplicateur de temps monotone (x1, x2, x4, x8) pour accélérer l’émulation.
- Le multiplicateur est injecté dans `VideoService` pour afficher l’étiquette SPD et dans le HAL pour `hal_timestamp_ms()`.
- La boucle principale reste cadencée par `tamalib_mainloop_step_by_step()` afin de conserver les timings du P1.

## Invariants techniques

- Les buffers `matrix_buffer` et `icon_buffer` résident dans `TamaHost` et ne sont modifiés que via `hal_set_lcd_matrix` / `hal_set_lcd_icon`; `VideoService` lit ces buffers et déclenche le rendu en fonction des flags «dirty».
- Seul `VideoService` est autorisé à dessiner des rectangles plein écran (clear + redraw localisés) afin de préserver l’anti-flicker et le throttling TFT ; les autres couches doivent passer par les callbacks HAL.
- La source de vérité du multiplicateur de temps est `TamaHost::timeMult_`, ajustée via `setTimeMultiplier()` ; la fonction rebascule `baseVirtualUs_` lors d’un changement (x4 → x1, etc.) pour garantir un temps virtuel strictement monotone sans à-coups.

Cette séparation garde la logique existante tout en rendant l’app maintenable pour des évolutions futures. Toute modification du HAL ou des périphériques doit être reflétée ici pour garder la documentation alignée.
