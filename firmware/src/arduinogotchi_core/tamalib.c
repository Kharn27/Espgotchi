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
#include "tamalib.h"
#include "hw.h"
#include "cpu.h"
#include "hal.h"

/* NB: on garde ton framerate par défaut à 3 fps */
#define DEFAULT_FRAMERATE 3 // fps

static exec_mode_t exec_mode = EXEC_MODE_RUN;
static u32_t step_depth = 0;
static timestamp_t screen_ts = 0;
static u32_t ts_freq;
static u8_t g_framerate = DEFAULT_FRAMERATE;

hal_t *g_hal;

bool_t tamalib_init(const u12_t *program,
					breakpoint_t *breakpoints,
					u32_t freq)
{
	bool_t res = 0;

	/* Pour l’instant, on ignore 'program' et 'breakpoints' et
	 * on garde le comportement historique : init CPU avec la
	 * seule fréquence.
	 *
	 * TODO: quand on branchera la ROM P1, on passera 'program'
	 *       et 'breakpoints' à cpu_init() comme dans TamaLIB upstream.
	 */
	(void)program;
	(void)breakpoints;

	res |= cpu_init(freq);
	res |= hw_init();

	ts_freq = freq;
	return res;
}

bool_t tamalib_init_espgotchi(u32_t freq)
{
	/* On passe désormais explicitement un programme et une liste
	 * de breakpoints à l’API TamaLIB, même si pour l’instant
	 * ce sont juste des NULL.
	 *
	 * Le comportement reste identique, car notre tamalib_init()
	 * ignore encore 'program' et 'breakpoints' et continue
	 * d’initialiser le CPU uniquement avec 'freq'.
	 */
	const u12_t *program = espgotchi_get_tama_program();
	breakpoint_t *breakpoints = espgotchi_get_tama_breakpoints();

	return tamalib_init(program, breakpoints, freq);
}

/* ---- API TamaLIB "officielle" restaurée ---- */

void tamalib_release(void)
{
	hw_release();
	cpu_release();
}

void tamalib_set_framerate(u8_t framerate)
{
	g_framerate = framerate;
}

u8_t tamalib_get_framerate(void)
{
	return g_framerate;
}

void tamalib_register_hal(hal_t *hal)
{
	g_hal = hal;
}

void tamalib_set_exec_mode(exec_mode_t mode)
{
	exec_mode = mode;
	step_depth = cpu_get_depth();
	cpu_sync_ref_timestamp();
}

void tamalib_step(void)
{
	if (exec_mode == EXEC_MODE_PAUSE)
	{
		return;
	}

	if (cpu_step())
	{
		/* CPU s'est arrêtée (halt) */
		exec_mode = EXEC_MODE_PAUSE;
		step_depth = cpu_get_depth();
	}
	else
	{
		switch (exec_mode)
		{
		case EXEC_MODE_PAUSE:
		case EXEC_MODE_RUN:
			/* on continue de tourner librement */
			break;

		case EXEC_MODE_STEP:
			/* un seul pas puis pause */
			exec_mode = EXEC_MODE_PAUSE;
			break;

		case EXEC_MODE_NEXT:
			/* run jusqu’à revenir à une profondeur <= step_depth */
			if (cpu_get_depth() <= step_depth)
			{
				exec_mode = EXEC_MODE_PAUSE;
				step_depth = cpu_get_depth();
			}
			break;

		case EXEC_MODE_TO_CALL:
			/* run jusqu’à ce qu’on descende dans la pile */
			if (cpu_get_depth() > step_depth)
			{
				exec_mode = EXEC_MODE_PAUSE;
				step_depth = cpu_get_depth();
			}
			break;

		case EXEC_MODE_TO_RET:
			/* run jusqu’à ce qu’on remonte dans la pile */
			if (cpu_get_depth() < step_depth)
			{
				exec_mode = EXEC_MODE_PAUSE;
				step_depth = cpu_get_depth();
			}
			break;
		}
	}
}

void tamalib_mainloop(void)
{
	timestamp_t ts;

	while (!g_hal->handler())
	{
		tamalib_step();

		/* Update the screen @ g_framerate fps */
		ts = g_hal->get_timestamp();
		if (ts - screen_ts >= ts_freq / g_framerate)
		{
			screen_ts = ts;
			g_hal->update_screen();
		}
	}
}

/* ---- Helper Espgotchi: boucle "step-by-step" ---- */

void tamalib_mainloop_step_by_step(void)
{
	timestamp_t ts;

	if (!g_hal->handler())
	{

		// Variante: exécuter seulement en mode RUN
		if (exec_mode == EXEC_MODE_RUN)
		{
			if (cpu_step())
			{
				exec_mode = EXEC_MODE_PAUSE;
				step_depth = cpu_get_depth();
			}
		}

		/* Update the screen @ g_framerate fps */
		ts = g_hal->get_timestamp();
		if (ts - screen_ts >= ts_freq / g_framerate)
		{
			screen_ts = ts;
			g_hal->update_screen();
		}
	}
}
