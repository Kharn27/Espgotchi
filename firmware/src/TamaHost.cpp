#include "TamaHost.h"
#include "VideoService.h"
#include "InputService.h"
#include "esp_timer.h"
#include <stdarg.h>

extern "C"
{
#include "cpu.h"
}

// pour le bouton debug centre écran
extern void printHeapStats();

// timeMult global : facteur de vitesse (x1, x2, x4, x8) pour TamaLIB
// et affichage de "SPD xN" dans la top bar.
uint8_t timeMult = 1;

TamaHost *TamaHost::s_instance = nullptr;

// HAL statique
hal_t TamaHost::s_hal = {
    .malloc = &TamaHost::hal_malloc,
    .free = &TamaHost::hal_free,
    .halt = &TamaHost::hal_halt,
    .is_log_enabled = &TamaHost::hal_is_log_enabled,
    .log = &TamaHost::hal_log,
    .sleep_until = &TamaHost::hal_sleep_until,
    .get_timestamp = &TamaHost::hal_get_timestamp,
    .update_screen = &TamaHost::hal_update_screen,
    .set_lcd_matrix = &TamaHost::hal_set_lcd_matrix,
    .set_lcd_icon = &TamaHost::hal_set_lcd_icon,
    .set_frequency = &TamaHost::hal_set_frequency,
    .play_frequency = &TamaHost::hal_play_frequency,
    .handler = &TamaHost::hal_handler,
};

TamaHost::TamaHost(VideoService &video, InputService &input)
    : _video(video), _input(input) {}

void TamaHost::begin(uint8_t displayFramerate, uint32_t startTimestampUs)
{
  s_instance = this;

  timeMult = 1;

  // On mémorise la fréquence utilisée pour TamaLIB (chez toi: 1_000_000 = us)
  _tamaTsFreq = startTimestampUs;
  _lastScreenUpdateTs = 0;

  tamalib_register_hal(&s_hal);
  tamalib_set_framerate(displayFramerate);
  tamalib_init_espgotchi(startTimestampUs);

  // S'assure qu'on démarre à vitesse x1
  cpu_set_speed(timeMult);

  Serial.println("[TamaHost] HAL registered, TamaLIB started.");
}

void TamaHost::loopOnce()
{
  // Équivalent à l’ancien tamalib_mainloop_step_by_step(), mais exprimé
  // uniquement via l’API publique TamaLIB + notre HAL Espgotchi.

  // 1. Handler d’événements (boutons, etc.) – même logique que g_hal->handler()
  if (!hal_handler())
  {
    // 2. On laisse TamaLIB décider quoi faire (RUN/PAUSE/STEP…) via tamalib_step().
    //    Si exec_mode == PAUSE, tamalib_step() ne fera rien – comme avant.
    tamalib_step();

    // 3. Rafraîchissement de l’écran à g_framerate fps
    timestamp_t ts = getTimestamp(); // équivalent à hal_get_timestamp()

    // Sécurité : si jamais _tamaTsFreq vaut 0 ou framerate vaut 0, on évite les divisions foireuses
    u32_t freq = _tamaTsFreq ? _tamaTsFreq : 1000000u;
    u8_t fr = tamalib_get_framerate();
    if (fr == 0)
    {
      fr = 1;
    }

    if (ts - _lastScreenUpdateTs >= freq / fr)
    {
      _lastScreenUpdateTs = ts;
      hal_update_screen();
    }
  }

  // log "alive" toutes les 2s – inchangé
  uint32_t nowMs = millis();
  if (nowMs - _lastAliveLogMs > 2000)
  {
    _lastAliveLogMs = nowMs;
    Serial.println("[Espgotchi] mainloop alive.");
  }
}

// -------- time scaling --------
void TamaHost::setTimeMult(uint8_t newMult)
{
  if (newMult < 1)
    newMult = 1;

  // Mémorise juste la valeur pour l’UI (SPD xN)
  timeMult = newMult;

  // Informe le CPU TamaLIB du nouveau ratio de vitesse.
  // 1 = vitesse normale, 2 = x2, 4 = x4, 8 = x8, etc.
  cpu_set_speed(timeMult);
}

timestamp_t TamaHost::getTimestamp()
{
  // Temps réel en microsecondes, directement depuis l’ESP.
  return (timestamp_t)esp_timer_get_time();
}

void TamaHost::sleepUntil(timestamp_t ts)
{
  // ts est exprimé dans la même base que getTimestamp()
  // (microsecondes réelles). On dort jusqu'à cette échéance.
  int64_t remaining = (int64_t)ts - (int64_t)esp_timer_get_time();
  if (remaining <= 0)
    return;

  if (remaining >= 2000)
  {
    uint32_t ms = (uint32_t)(remaining / 1000);
    if (ms > 0)
      delay(ms);
  }

  remaining = (int64_t)ts - (int64_t)esp_timer_get_time();
  if (remaining > 0)
  {
    delayMicroseconds((uint32_t)remaining);
  }
}

// -------- HAL instance -> méthodes --------

void TamaHost::handleUpdateScreen()
{
  _video.updateScreen();
}

void TamaHost::handleSetLcdMatrix(u8_t x, u8_t y, bool_t val)
{
  _video.setLcdMatrix(x, y, val);
}

void TamaHost::handleSetLcdIcon(u8_t icon, bool_t val)
{
  _video.setLcdIcon(icon, val);
}

void TamaHost::handleSetFrequency(u32_t freq_dHz)
{
  // TamaLIB envoie une fréquence en décis-Hz (dHz).
  // On convertit en Hz pour la couche audio ESP.
  extern void espgotchi_hal_set_frequency(u32_t freqHz);

  u32_t freqHz = freq_dHz / 10; // 40960 dHz -> 4096 Hz, etc.
  espgotchi_hal_set_frequency(freqHz);
}

void TamaHost::handlePlayFrequency(bool_t en)
{
  extern void espgotchi_hal_play_frequency(bool_t en);
  espgotchi_hal_play_frequency(en);
}

int TamaHost::handleHandler()
{
  // 1) input -> hw_set_button()
  _input.update();

  // 2) bouton SPD (tap logique géré par InputService)
  if (_input.consumeTap(LogicalButton::SPEED))
  {
    uint8_t next =
        (timeMult == 1) ? 2 : (timeMult == 2) ? 4
                          : (timeMult == 4)   ? 8
                                              : 1;

    setTimeMult(next);
    Serial.printf("[Time] Speed x%d\n", timeMult);
  }

  // 3) bouton debug au centre de l'écran
  if (_input.consumeTap(LogicalButton::DEBUG_CENTER))
  {
    printHeapStats();
  }

  // 4) log HELD propre (on utilise maintenant LogicalButton)
  LogicalButton held = _input.getHeld();
  uint8_t heldRaw = static_cast<uint8_t>(held);

  if (heldRaw != _lastHeldLogged)
  {
    _lastHeldLogged = heldRaw;

    switch (held)
    {
    case LogicalButton::LEFT:
      Serial.println("[Input] HELD LEFT");
      break;
    case LogicalButton::OK:
      Serial.println("[Input] HELD OK");
      break;
    case LogicalButton::RIGHT:
      Serial.println("[Input] HELD RIGHT");
      break;
    default:
      Serial.println("[Input] HELD NONE");
      break;
    }
  }

  return 0;
}

// -------- HAL statiques --------

void *TamaHost::hal_malloc(u32_t size)
{
  // Pour l’instant on n’utilise pas les breakpoints,
  // donc on peut renvoyer NULL et tamalib ne devrait
  // pas appeler cette fonction dans notre config.
  (void)size;
  return NULL;
}

void TamaHost::hal_free(void *ptr)
{
  // Rien à faire tant qu’on ne fait pas d’alloc dynamique via le HAL
  (void)ptr;
}

bool_t TamaHost::hal_is_log_enabled(log_level_t level)
{
  // On limite les logs à ERROR et INFO pour éviter le flood.
  if (level & (LOG_ERROR | LOG_INFO))
  {
    return 1;
  }
  return 0;

  // // Implémentation simple : tout est loggué.
  // // On pourra affiner plus tard (filtrer LOG_MEMORY, LOG_CPU, etc.)
  // (void)level;
  // return 1;
}

void TamaHost::hal_halt()
{
  // rien de spécial
}

void TamaHost::hal_log(log_level_t level, char *buff, ...)
{
  // On ne garde que ERROR (et éventuellement INFO).
  // LOG_MEMORY, LOG_CPU, LOG_INT sont ignorés pour éviter le flood.
  if ((level & (LOG_ERROR | LOG_INFO)) == 0)
  {
    return;
  }

  if (!buff)
  {
    return;
  }

  char formatted[256];

  va_list args;
  va_start(args, buff);
  vsnprintf(formatted, sizeof(formatted), buff, args);
  va_end(args);

  // TamaLIB appelle log() plusieurs fois pour construire une ligne :
  // on utilise print() plutôt que println() pour respecter ça.
  Serial.print(formatted);
}

timestamp_t TamaHost::hal_get_timestamp()
{
  return s_instance ? s_instance->getTimestamp() : 0;
}

void TamaHost::hal_sleep_until(timestamp_t ts)
{
  if (s_instance)
    s_instance->sleepUntil(ts);
}

void TamaHost::hal_update_screen()
{
  if (s_instance)
    s_instance->handleUpdateScreen();
}

void TamaHost::hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  if (s_instance)
    s_instance->handleSetLcdMatrix(x, y, val);
}

void TamaHost::hal_set_lcd_icon(u8_t icon, bool_t val)
{
  if (s_instance)
    s_instance->handleSetLcdIcon(icon, val);
}

void TamaHost::hal_set_frequency(u32_t freq)
{
  if (s_instance)
    s_instance->handleSetFrequency(freq);
}

void TamaHost::hal_play_frequency(bool_t en)
{
  if (s_instance)
    s_instance->handlePlayFrequency(en);
}

int TamaHost::hal_handler()
{
  return s_instance ? s_instance->handleHandler() : 0;
}
