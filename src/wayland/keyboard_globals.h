#ifndef INCLUDE_KEYBOARD_GLOBALS_H
#define INCLUDE_KEYBOARD_GLOBALS_H
#include "../kbd_drv.h"
#include "state.h"

void expand_key_history (struct ClientState *state, uint32_t key);
#endif