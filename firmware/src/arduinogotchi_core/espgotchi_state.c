#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hal.h"
#include "cpu.h"
#include "arduinogotchi_core/espgotchi_state.h"

/*
 * Espgotchi logical state inspection module (P1 specific)
 * -------------------------------------------------------
 * This module reads the emulated Vpet RAM from TamaLIB to expose a stable API
 * for higher layers. Offsets and layout are encapsulated here to avoid leaking
 * TamaLIB internals to the rest of the codebase.
 */

static void state_log(const char *fmt, ...)
{
    if ((g_hal == NULL) || (g_hal->log == NULL)) {
        return;
    }

    if ((g_hal->is_log_enabled != NULL) && (g_hal->is_log_enabled(LOG_INFO) == 0)) {
        return;
    }

    char buffer[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    g_hal->log(LOG_INFO, "%s", buffer);
}

void espgotchi_read_logical_state(espgotchi_logical_state_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
}

void espgotchi_debug_dump_state(const espgotchi_logical_state_t *st)
{
    if (st == NULL) {
        return;
    }

    state_log("[Espgotchi][state] hunger=%u happy=%u discipline=%u", st->hunger_hearts, st->happiness_hearts,
              st->discipline_hearts);
    state_log("[Espgotchi][state] age=%u weight=%u gen=%u sick=%u poop=%u sleep=%u lights_off=%u dead=%u",
              st->age_days, st->weight_oz, st->generation, st->sickness, st->has_poop, st->is_sleeping,
              st->lights_off, st->is_dead);
    state_log("[Espgotchi][state] clock=%02u:%02u paused=%u", st->pet_hour, st->pet_minute, st->is_paused);
}

