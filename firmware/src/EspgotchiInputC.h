#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 0 = NONE, 1 = LEFT, 2 = OK, 3 = RIGHT
void espgotchi_input_begin();
void espgotchi_input_update();

uint8_t espgotchi_input_consume_any();
uint8_t espgotchi_input_peek_held();
uint8_t espgotchi_input_get_last_touch(uint16_t* x, uint16_t* y, uint8_t* down);


#ifdef __cplusplus
}
#endif
