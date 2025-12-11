#include "espgotchi_tama_rom.h"

/* NOTE:
 * Dans une étape future, on retournera ici le vrai programme P1
 * (tableau de u12_t généré depuis la ROM, comme rom_12bit.h
 *  dans ArduinoGotchi), et éventuellement une liste de breakpoints.
 */

const u12_t *espgotchi_get_tama_program(void)
{
    // Pour l’instant, aucun programme explicite : on laisse NULL.
    return NULL;
}

breakpoint_t *espgotchi_get_tama_breakpoints(void)
{
    // Pas de breakpoints gérés côté Espgotchi pour l’instant.
    return NULL;
}
