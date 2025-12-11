#include "espgotchi_tamalib_ext.h"

bool_t tamalib_init_espgotchi(u32_t freq)
{
    const u12_t *program      = espgotchi_get_tama_program();
    breakpoint_t *breakpoints = espgotchi_get_tama_breakpoints();

    return tamalib_init(program, breakpoints, freq);
}
/* ---- API TamaLIB "officielle" restaurée ---- */