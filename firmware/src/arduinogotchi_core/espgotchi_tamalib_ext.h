#ifndef _ESPGOTCHI_TAMALIB_EXT_H_
#define _ESPGOTCHI_TAMALIB_EXT_H_

#include "tamalib.h"
#include "cpu.h"
#include "espgotchi_tama_rom.h"

#ifdef __cplusplus
extern "C" {
#endif

// Init spécifique Espgotchi (wrapper autour de tamalib_init)
bool_t tamalib_init_espgotchi(u32_t freq);

#ifdef __cplusplus
}
#endif

#endif /* _ESPGOTCHI_TAMALIB_EXT_H_ */
