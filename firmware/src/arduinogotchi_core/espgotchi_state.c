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

// P1 core stats layout (d'après https://gist.github.com/Notunderscore/67646086b2d035027b0383f491e2278b)
#define P1_RAM_ADDR_FULLNESS 0x0040       // "hunger" hearts
#define P1_RAM_ADDR_HAPPINESS 0x0041      // "happy" hearts
#define P1_RAM_ADDR_CARE_MISTAKES 0x0042  // non décodé pour l'instant
#define P1_RAM_ADDR_DISCIPLINE 0x0043
#define P1_RAM_ADDR_WEIGHT_LSB 0x0046     // poids en décimal (little-endian BCD)
#define P1_RAM_ADDR_WEIGHT_MSB 0x0047
#define P1_RAM_ADDR_SICK_COUNTER 0x0049   // lié aux maladies (indicatif uniquement)
#define P1_RAM_ADDR_LIGHTS 0x004B         // 0 = éteintes, F = allumées
#define P1_RAM_ADDR_POOP_COUNT 0x004D
#define P1_RAM_ADDR_STAGE 0x0050          // type / étape d'évolution
#define P1_RAM_ADDR_AGE_LSB 0x0054        // âge en jours (little-endian BCD)
#define P1_RAM_ADDR_AGE_MSB 0x0055

// Flags sommeil (observés comme complémentaires)
#define P1_RAM_ADDR_SLEEP_FLAG_PRIMARY 0x0231 // 1 = endormi, 0 = éveillé ?
#define P1_RAM_ADDR_SLEEP_FLAG_SECONDARY 0x0233

static u8_t s_last_snapshot[P1_RAM_MAX];
static int s_has_snapshot = 0;

static u8_t p1_bcd_to_uint(u8_t tens, u8_t units)
{
    return (tens & 0x0F) * 10u + (units & 0x0F);
}

// Convertit un entier décimal encodé sur 4 nibbles "little-endian" (ex: 1234 -> 4321)
static u16_t p1_bcd16_le_to_uint(u8_t byte0, u8_t byte1)
{
    u16_t n0 = (u16_t)(byte0 & 0x0F);
    u16_t n1 = (u16_t)((byte0 >> 4) & 0x0F);
    u16_t n2 = (u16_t)(byte1 & 0x0F);
    u16_t n3 = (u16_t)((byte1 >> 4) & 0x0F);

    return n0 + (n1 * 10u) + (n2 * 100u) + (n3 * 1000u);
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

    // --- Statistiques principales ---
    u8_t fullness_raw = p1_ram_read(P1_RAM_ADDR_FULLNESS);
    u8_t happiness_raw = p1_ram_read(P1_RAM_ADDR_HAPPINESS);
    u8_t discipline_raw = p1_ram_read(P1_RAM_ADDR_DISCIPLINE);
    u8_t weight_lsb = p1_ram_read(P1_RAM_ADDR_WEIGHT_LSB);
    u8_t weight_msb = p1_ram_read(P1_RAM_ADDR_WEIGHT_MSB);
    u8_t age_lsb = p1_ram_read(P1_RAM_ADDR_AGE_LSB);
    u8_t age_msb = p1_ram_read(P1_RAM_ADDR_AGE_MSB);
    u8_t poop_count = p1_ram_read(P1_RAM_ADDR_POOP_COUNT);
    u8_t lights_raw = p1_ram_read(P1_RAM_ADDR_LIGHTS);
    u8_t sleep_raw_primary = p1_ram_read(P1_RAM_ADDR_SLEEP_FLAG_PRIMARY);
    u8_t sleep_raw_secondary = p1_ram_read(P1_RAM_ADDR_SLEEP_FLAG_SECONDARY);
    u8_t sickness_counter = p1_ram_read(P1_RAM_ADDR_SICK_COUNTER);
    u8_t stage_raw = p1_ram_read(P1_RAM_ADDR_STAGE);

    u16_t weight_val = p1_bcd16_le_to_uint(weight_lsb, weight_msb);
    u16_t age_val = p1_bcd16_le_to_uint(age_lsb, age_msb);

    if (weight_val > UINT8_MAX)
    {
        weight_val = UINT8_MAX;
    }
    if (age_val > UINT8_MAX)
    {
        age_val = UINT8_MAX;
    }

    out->hunger_hearts = fullness_raw & 0x0F;
    out->happiness_hearts = happiness_raw & 0x0F;
    out->discipline_hearts = discipline_raw & 0x0F;
    out->weight_oz = (u8_t)weight_val;
    out->age_days = (u8_t)age_val;
    out->has_poop = poop_count ? 1 : 0;
    out->lights_off = (lights_raw == 0) ? 1 : 0;
    out->is_sleeping = (sleep_raw_primary || sleep_raw_secondary) ? 1 : 0;
    out->sickness = (sickness_counter != 0) ? 1 : 0;
    out->generation = stage_raw; // valeur brute pour validation

    state_log("[stats raw] full=0x%X happy=0x%X disc=0x%X weight_le=0x%X%X age_le=0x%X%X poop=0x%X lights=0x%X sleep=%u/%u sick=0x%X stage=0x%X",
              fullness_raw, happiness_raw, discipline_raw,
              weight_msb, weight_lsb, age_msb, age_lsb,
              poop_count, lights_raw,
              sleep_raw_primary, sleep_raw_secondary,
              sickness_counter, stage_raw);

    state_log("[stats logical] hunger=%u happy=%u discipline=%u weight_oz=%u age_days=%u poop=%u lights_off=%u sleeping=%u sickness=%u generation_raw=%u",
              out->hunger_hearts, out->happiness_hearts, out->discipline_hearts,
              out->weight_oz, out->age_days, out->has_poop,
              out->lights_off, out->is_sleeping, out->sickness, out->generation);
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

void espgotchi_state_reset_snapshot(void)
{
    s_has_snapshot = 0;
}

// Dump ciblé d'une plage [start, end)
void espgotchi_state_dump_ram_range(uint16_t start, uint16_t end)
{
    if (start >= end || end > MEM_RAM_SIZE)
        return;

    u8_t current[P1_RAM_MAX];

    for (uint16_t addr = 0; addr < P1_RAM_MAX; ++addr)
        current[addr] = p1_ram_read(addr);

    if (!s_has_snapshot) {
        memcpy(s_last_snapshot, current, P1_RAM_MAX);
        s_has_snapshot = 1;
        state_log("[Espgotchi][state] RAM snapshot initialised (custom range)");
        return;
    }

    state_log("---- RAM diff dump (0x%03X..0x%03X) ----",
              (unsigned int)start, (unsigned int)(end - 1));

    for (uint16_t addr = start; addr < end; ++addr) {
        if (current[addr] != s_last_snapshot[addr]) {
            state_log("[RAM] addr=0x%03X: 0x%X -> 0x%X",
                      addr, s_last_snapshot[addr], current[addr]);
        }
    }

    memcpy(s_last_snapshot, current, P1_RAM_MAX);
}
