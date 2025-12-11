#include "espgotchi_tama_rom.h"

/* rom_12bit.h utilise PROGMEM (Arduino) qui n'est pas défini
 * dans ce TU en C pur. On le neutralise ici.
 */
#ifndef PROGMEM
#define PROGMEM
#endif
#include "rom_12bit.h"  // contient g_program_b12 = ROM P1 packée en 12 bits

/* NOTE:
 * Dans une étape future, on retournera ici le vrai programme P1
 * (tableau de u12_t généré depuis la ROM, comme rom_12bit.h
 *  dans ArduinoGotchi), et éventuellement une liste de breakpoints.
 */
const u12_t *espgotchi_get_tama_program(void)
{
    /* On a déjà la ROM P1 packée en 12 bits dans rom_12bit.h :
     *   static const unsigned char g_program_b12[] PROGMEM;
     *
     * TamaLIB s’attend à recevoir un tableau de u12_t (une
     * instruction par entrée). Pour l’instant, notre cpu.c
     * n’utilise PAS ce pointeur 'program' et lit directement
     * g_program_b12 via getProgramOpCode().
     *
     * Du coup, on peut se contenter de passer un pointeur
     * symbolique vers cette ROM (castée), sans changer le
     * comportement. Le jour où on migrera sur le CPU upstream,
     * on utilisera vraiment ce pointeur pour décoder la ROM
     * en u12_t.
     */

    return (const u12_t *)(const void *)g_program_b12;
}


breakpoint_t *espgotchi_get_tama_breakpoints(void)
{
    // Pas de breakpoints gérés côté Espgotchi pour l’instant.
    return NULL;
}
