#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>
// USB HID boot-protocol keys -> MSX keyboard matrix (11 rows, bit set = key pressed)
void hidToMatrix(uint8_t modifiers, const uint8_t keys[6], uint8_t matrix[11]);
#endif
