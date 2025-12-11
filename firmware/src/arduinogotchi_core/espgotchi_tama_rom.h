#ifndef _ESPGOTCHI_TAMA_ROM_H_
#define _ESPGOTCHI_TAMA_ROM_H_

#include "cpu.h"   // pour u12_t et breakpoint_t

#ifdef __cplusplus
extern "C" {
#endif

/* Retourne le pointeur sur le programme Tama (ROM P1).
 * Pour l’instant : NULL, on recâblera plus tard vers la vraie ROM.
 */
const u12_t *espgotchi_get_tama_program(void);
u32_t espgotchi_get_tama_program_word_count(void);

/* Retourne la liste de breakpoints (ou NULL).
 * Pour l’instant, on n’utilise pas de breakpoints côté Espgotchi.
 */
breakpoint_t *espgotchi_get_tama_breakpoints(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESPGOTCHI_TAMA_ROM_H_ */
