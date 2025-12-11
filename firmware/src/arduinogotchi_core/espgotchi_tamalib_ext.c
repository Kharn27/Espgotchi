#include <assert.h>
#include <stdint.h>
#include "esp_rom_sys.h"

#include "espgotchi_tamalib_ext.h"

static bool_t espgotchi_validate_breakpoints(const breakpoint_t *breakpoints)
{
    if (breakpoints == NULL) {
        return 1;
    }

    /* Basic sanity check: the pointer must be aligned on breakpoint_t. */
    if (((uintptr_t)breakpoints % sizeof(breakpoint_t)) != 0u) {
        esp_rom_printf("[TamaLIB] Invalid breakpoint pointer alignment: %p\n", (void *)breakpoints);
        assert(((uintptr_t)breakpoints % sizeof(breakpoint_t)) == 0u);
        return 0;
    }

    return 1;
}

bool_t tamalib_init_espgotchi(u32_t freq)
{
    const u12_t *program        = espgotchi_get_tama_program();
    const u32_t program_words   = espgotchi_get_tama_program_word_count();
    breakpoint_t *breakpoints   = espgotchi_get_tama_breakpoints();
    const bool_t program_valid  = (program != NULL) && (program_words > 0u);
    const bool_t breakpoints_ok = espgotchi_validate_breakpoints(breakpoints);

    if (!program_valid) {
        esp_rom_printf("[TamaLIB] Invalid ROM buffer (program=%p, words=%lu)\n", (void *)program,
                       (unsigned long)program_words);
        assert(program != NULL);
        assert(program_words > 0u);
        return 0;
    }

    if (!breakpoints_ok) {
        return 0;
    }

    return tamalib_init(program, breakpoints, freq);
}
/* ---- API TamaLIB "officielle" restaurée ---- */
