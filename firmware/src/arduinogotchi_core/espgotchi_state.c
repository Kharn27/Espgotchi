#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h> // pour uint16_t

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

// On inspecte uniquement la RAM "logique" du Tama (pas les registres IO)
#define P1_RAM_MAX MEM_RAM_SIZE
// On ne s'intéresse qu'à une petite zone pour la clock (0x0000 - 0x0040)
#define DEBUG_RAM_START 0x0000
#define DEBUG_RAM_END 0x0040 // non inclus, donc 0x0000..0x003F

// P1 clock layout (hypothèse basée sur diff RAM avec horloge réglée à 05:00)
// P1 clock layout (validé par diff RAM)
#define P1_RAM_ADDR_CLOCK_SEC_U 0x0010  // secondes unités  (0..9)
#define P1_RAM_ADDR_CLOCK_SEC_T 0x0011  // secondes dizaines (0..5)
#define P1_RAM_ADDR_CLOCK_MIN_U 0x0012  // minutes unités   (0..9)
#define P1_RAM_ADDR_CLOCK_MIN_T 0x0013  // minutes dizaines (0..5)
#define P1_RAM_ADDR_CLOCK_HOUR_U 0x0014 // heures unités    (0..9)
#define P1_RAM_ADDR_CLOCK_HOUR_T 0x0015 // heures dizaines  (0..2)

static u8_t s_last_snapshot[P1_RAM_MAX];
static int s_has_snapshot = 0;

static u8_t p1_bcd_to_uint(u8_t tens, u8_t units)
{
    return (tens & 0x0F) * 10u + (units & 0x0F);
}

// Heure encodée en binaire sur 2 nibbles (0..23)
static u8_t p1_hour_to_uint(u8_t ht, u8_t hu)
{
    return ((ht & 0x0F) << 4) | (hu & 0x0F); // ht*16 + hu
}

static void state_log(const char *fmt, ...)
{
    if ((g_hal == NULL) || (g_hal->log == NULL))
    {
        return;
    }

    if ((g_hal->is_log_enabled != NULL) && (g_hal->is_log_enabled(LOG_INFO) == 0))
    {
        return;
    }

    char buffer[160];
    va_list args;
    va_start(args, fmt);
    // on garde 2 caractères libres pour "\n\0"
    vsnprintf(buffer, sizeof(buffer) - 2, fmt, args);
    va_end(args);

    // Ajout manuel du retour à la ligne
    size_t len = strlen(buffer);
    buffer[len++] = '\n';
    buffer[len] = '\0';

    // hal_log attend un char* pour fmt, pas const char*
    g_hal->log(LOG_INFO, (char *)"%s", buffer);
}

// Lecture d'un "nibble" de RAM (4 bits logiques) à l'offset donné dans la zone RAM
static u8_t p1_ram_read(uint16_t offset)
{
    state_t *st = cpu_get_state();
    if (st == NULL || st->memory == NULL)
    {
        return 0;
    }

    if (offset >= MEM_RAM_SIZE)
    {
        return 0;
    }

    // Adresse mémoire logique = base RAM + offset
    uint16_t mem_addr = MEM_RAM_ADDR + offset;

    // GET_RAM_MEMORY est fourni par cpu.h et gère le mapping LOW_FOOTPRINT
    return GET_RAM_MEMORY(st->memory, mem_addr);
}

static void debug_dump_ram_diff(void)
{
    static uint32_t s_dump_index = 0;

    u8_t current[P1_RAM_MAX];

    for (uint16_t addr = 0; addr < P1_RAM_MAX; ++addr)
    {
        current[addr] = p1_ram_read(addr);
    }

    if (!s_has_snapshot)
    {
        memcpy(s_last_snapshot, current, P1_RAM_MAX);
        s_has_snapshot = 1;
        state_log("[Espgotchi][state] RAM snapshot initialised");
        return;
    }

    state_log("---- RAM diff dump #%lu ----", (unsigned long)s_dump_index++);

    for (uint16_t addr = DEBUG_RAM_START; addr < DEBUG_RAM_END; ++addr)
    {
        if (current[addr] != s_last_snapshot[addr])
        {
            state_log("[RAM] addr=0x%03X: 0x%X -> 0x%X",
                      addr, s_last_snapshot[addr], current[addr]);
        }
    }

    memcpy(s_last_snapshot, current, P1_RAM_MAX);
}

void espgotchi_read_logical_state(espgotchi_logical_state_t *out)
{
    if (out == NULL)
    {
        return;
    }

    memset(out, 0, sizeof(*out));

    // --- Horloge P1 ---
    // secondes / minutes en BCD (tens/units)
    // heures encodées en entier binaire sur 2 nibbles (0..23) via p1_hour_to_uint()
    u8_t sec_u = p1_ram_read(P1_RAM_ADDR_CLOCK_SEC_U);
    u8_t sec_t = p1_ram_read(P1_RAM_ADDR_CLOCK_SEC_T);
    u8_t min_u = p1_ram_read(P1_RAM_ADDR_CLOCK_MIN_U);
    u8_t min_t = p1_ram_read(P1_RAM_ADDR_CLOCK_MIN_T);
    u8_t hr_u = p1_ram_read(P1_RAM_ADDR_CLOCK_HOUR_U);
    u8_t hr_t = p1_ram_read(P1_RAM_ADDR_CLOCK_HOUR_T);

    out->pet_hour = p1_hour_to_uint(hr_t, hr_u);
    out->pet_minute = p1_bcd_to_uint(min_t, min_u);
    out->pet_second = p1_bcd_to_uint(sec_t, sec_u);
    // out->is_paused  = 0;

    state_log("[clock raw] ht=%u hu=%u mt=%u mu=%u st=%u su=%u -> hour=%u min=%u",
              hr_t, hr_u, min_t, min_u, sec_t, sec_u,
              out->pet_hour, out->pet_minute);

    // Pour l'instant, on utilise cette fonction uniquement pour le debug RAM
    // debug_dump_ram_diff();
}

void espgotchi_debug_dump_state(const espgotchi_logical_state_t *st)
{
    if (st == NULL)
    {
        return;
    }

    state_log("[Espgotchi][state] hunger=%u happy=%u discipline=%u",
              st->hunger_hearts, st->happiness_hearts, st->discipline_hearts);
    state_log("[Espgotchi][state] age=%u weight=%u gen=%u sick=%u poop=%u sleep=%u lights_off=%u dead=%u",
              st->age_days, st->weight_oz, st->generation, st->sickness,
              st->has_poop, st->is_sleeping, st->lights_off, st->is_dead);
    state_log("[Espgotchi][state] clock=%02u:%02u:%02u paused=%u",
              st->pet_hour, st->pet_minute, st->pet_second, st->is_paused);
}
