#include "espgotchi_tama_rom.h"

/* rom_12bit.h utilise PROGMEM (Arduino) qui n'est pas défini
 * dans ce TU en C pur. On le neutralise ici.
 */
#ifndef PROGMEM
#define PROGMEM
#endif

#include "rom_12bit.h"  // contient g_program_b12 = ROM P1 packée en 12 bits

/* Nombre d'opcodes 12 bits dans la ROM :
 * Chaque paire d'opcodes est packée sur 3 octets.
 */
#define ESPGOTCHI_PROGRAM_WORDS ((sizeof(g_program_b12) / 3) * 2)

/* Programme TamaLIB au format attendu par cpu.c (un u12_t par opcode) */
static u12_t s_program[ESPGOTCHI_PROGRAM_WORDS];
static bool_t s_program_initialized = 0;

/* Dépacke g_program_b12 (3 octets -> 2 opcodes 12 bits) dans s_program[] */
static void espgotchi_build_program(void)
{
    if (s_program_initialized) {
        return;
    }

    const u32_t byte_count = (u32_t)sizeof(g_program_b12);
    const u32_t pair_count = byte_count / 3;
    const u32_t op_count   = pair_count * 2;  // = ESPGOTCHI_PROGRAM_WORDS

    for (u32_t pc = 0; pc < op_count; ++pc) {
        u32_t i = pc >> 1;     // index de paire (2 opcodes / 3 octets)
        u32_t j = i * 3;       // index de base dans g_program_b12

        if ((pc & 1u) == 0u) {
            /* opcode pair : bits 11..0 = [byte0 << 4] | [byte1 bits 7..4] */
            s_program[pc] =
                ((u12_t)g_program_b12[j] << 4) |
                ((u12_t)(g_program_b12[j + 1] >> 4) & 0x0F);
        } else {
            /* opcode impair : bits 11..0 = [byte1 bits 3..0 << 8] | byte2 */
            s_program[pc] =
                ((u12_t)(g_program_b12[j + 1] & 0x0F) << 8) |
                (u12_t)g_program_b12[j + 2];
        }
    }

    s_program_initialized = 1;
}

const u12_t *espgotchi_get_tama_program(void)
{
    /* Première fois : on dépacke la ROM packée 12 bits dans s_program[] */
    espgotchi_build_program();
    return s_program;
}

u32_t espgotchi_get_tama_program_word_count(void)
{
    return ESPGOTCHI_PROGRAM_WORDS;
}

breakpoint_t *espgotchi_get_tama_breakpoints(void)
{
    // Pas de breakpoints gérés côté Espgotchi pour l’instant.
    return NULL;
}
