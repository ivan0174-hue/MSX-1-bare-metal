#include "keyboard.h"

struct KMap { uint8_t hid, row, bit; };

static const KMap kMap[] = {
	{0x27,0,0},{0x1E,0,1},{0x1F,0,2},{0x20,0,3},{0x21,0,4},{0x22,0,5},{0x23,0,6},{0x24,0,7},
	{0x25,1,0},{0x26,1,1},{0x2D,1,2},{0x2E,1,3},{0x31,1,4},{0x2F,1,5},{0x30,1,6},{0x33,1,7},
	{0x34,2,0},{0x35,2,1},{0x36,2,2},{0x37,2,3},{0x38,2,4},
	{0x39,6,3},{0x3A,6,5},{0x3B,6,6},{0x3C,6,7},{0x3D,7,0},{0x3E,7,1},
	{0x29,7,2},{0x2B,7,3},{0x48,7,4},{0x3F,7,4},{0x2A,7,5},{0x4D,7,6},{0x40,7,6},{0x28,7,7},
	{0x41,6,4},
	{0x2C,8,0},{0x4A,8,1},{0x49,8,2},{0x4C,8,3},{0x50,8,4},{0x52,8,5},{0x51,8,6},{0x4F,8,7},
	{0x55,9,0},{0x57,9,1},{0x54,9,2},{0x62,9,3},{0x59,9,4},{0x5A,9,5},{0x5B,9,6},{0x5C,9,7},
	{0x5D,10,0},{0x5E,10,1},{0x5F,10,2},{0x60,10,3},{0x61,10,4},{0x56,10,5},{0x63,10,7},
	{0x58,7,7}
};

void hidToMatrix(uint8_t mods, const uint8_t keys[6], uint8_t m[11])
{
	for (int i = 0; i < 11; i++) m[i] = 0;
	if (mods & 0x22) m[6] |= 0x01;		// shift
	if (mods & 0x11) m[6] |= 0x02;		// ctrl
	if (mods & 0x04) m[6] |= 0x04;		// left alt  -> GRAPH
	if (mods & 0x40) m[6] |= 0x10;		// right alt -> CODE
	for (int k = 0; k < 6; k++) {
		uint8_t u = keys[k];
		if (!u) continue;
		if (u >= 0x04 && u <= 0x1D) {	// letters
			int n = u - 0x04;
			if (n == 0) m[2] |= 0x40;
			else if (n == 1) m[2] |= 0x80;
			else { int q = n - 2; m[3 + q / 8] |= 1 << (q % 8); }
			continue;
		}
		for (unsigned j = 0; j < sizeof kMap / sizeof kMap[0]; j++)
			if (kMap[j].hid == u) m[kMap[j].row] |= 1 << kMap[j].bit;
	}
}
