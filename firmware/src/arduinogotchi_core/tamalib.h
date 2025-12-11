/*
 * TamaLIB - A hardware agnostic Tamagotchi P1 emulation library
 *
 * Copyright (C) 2021 Jean-Christophe Rona <jc@rona.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef _TAMALIB_H_
#define _TAMALIB_H_

#include "cpu.h"
#include "hw.h"
#include "hal.h"


/* Macros de façade comme dans TamaLIB upstream */
#define tamalib_set_button(btn, state)   hw_set_button((btn), (state))
#define tamalib_set_speed(speed)         cpu_set_speed((speed))
#define tamalib_get_state()              cpu_get_state()
#define tamalib_refresh_hw()             cpu_refresh_hw()
#define tamalib_reset()                  cpu_reset()
#define tamalib_add_bp(list, addr)       cpu_add_bp((list), (addr))
#define tamalib_free_bp(list)            cpu_free_bp((list))

typedef enum {
	EXEC_MODE_PAUSE,
	EXEC_MODE_RUN,
	EXEC_MODE_STEP,
	EXEC_MODE_NEXT,
	EXEC_MODE_TO_CALL,
	EXEC_MODE_TO_RET,
} exec_mode_t;


#ifdef __cplusplus
 extern "C" {
#endif

/* API TamaLIB "officielle" (sauf init pour l'instant) */
void   tamalib_release(void);

/* Init simplifiée actuelle d'Espgotchi
 * NOTE: on gardera ce prototype tant qu’on n’a pas recâblé la ROM.
 */
bool_t tamalib_init(u32_t freq);

void   tamalib_set_framerate(u8_t framerate);
u8_t   tamalib_get_framerate(void);
/* Init spécifique Espgotchi (wrapper autour de tamalib_init)
 * Permettra plus tard de faire évoluer tamalib_init() vers la signature upstream
 * sans casser l’API du projet.
 */
bool_t tamalib_init_espgotchi(u32_t freq);
void   tamalib_register_hal(hal_t *hal);
void   tamalib_set_exec_mode(exec_mode_t mode);

/* NOTE: Only one of these two functions must be used in the main application
 * (tamalib_step() should be used only if tamalib_mainloop() does not fit the
 * main application execution flow).
 */
void   tamalib_step(void);
void   tamalib_mainloop(void);

/* Helper spécifique Espgotchi : boucle step-by-step à 3 fps */
void   tamalib_mainloop_step_by_step(void);

#ifdef __cplusplus
}
#endif

#endif /* _TAMALIB_H_ */