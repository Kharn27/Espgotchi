#pragma once

#include "../../lib/hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Logical status of the virtual pet */
    u8_t hunger_hearts;    /* 0..4 (depending on ROM) */
    u8_t happiness_hearts; /* 0..4 */
    u8_t discipline_hearts;/* 0..4 */

    /* Misc states */
    u8_t age_days;
    u8_t weight_oz;
    u8_t generation;
    u8_t sickness;   /* boolean flag */
    u8_t has_poop;   /* boolean flag */
    u8_t is_sleeping;/* boolean flag */
    u8_t lights_off; /* boolean flag */
    u8_t is_dead;    /* boolean flag */

    /* Internal Vpet clock */
    u8_t pet_hour;   /* 0..23 or 0..12 depending on ROM */
    u8_t pet_minute; /* 0..59 */
    u8_t pet_second; /* 0..59 */
    u8_t is_paused;  /* boolean flag */
} espgotchi_logical_state_t;

/* Populate the structure by reading the emulated Vpet RAM */
void espgotchi_read_logical_state(espgotchi_logical_state_t *out);

/* Debug helper that logs the current logical state */
void espgotchi_debug_dump_state(const espgotchi_logical_state_t *st);

// Optionnel : API de debug pour contrôler les dumps RAM
void espgotchi_state_reset_snapshot(void);
void espgotchi_state_dump_ram_range(uint16_t start, uint16_t end);


#ifdef __cplusplus
}
#endif

