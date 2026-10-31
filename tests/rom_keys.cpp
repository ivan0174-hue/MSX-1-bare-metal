#include <stdlib.h>
#include "msx.h"
#include <stdio.h>
#include <map>
struct M : Msx {
	std::map<int,int> rows; std::map<int,int> joy; uint8_t psg15 = 0;
	uint8_t in(uint16_t p) { uint8_t v = Msx::in(p); if ((p & 255) == 0xA9) rows[ppiC & 15]++; if ((p & 255) == 0xA2 && psg.selected() == 14) joy[psg15]++; return v; }
	void out(uint16_t p, uint8_t v) { if ((p & 255) == 0xA1 && psg.selected() == 15) psg15 = v; Msx::out(p, v); }
};
int main(int argc, char **argv)
{
	static uint8_t rom[0x10000]; FILE *f = fopen(argv[1], "rb"); size_t n = fread(rom, 1, sizeof rom, f);
	M m; m.setBios(rom, n); m.hardReset(); m.ppiA = 0xF0; m.pc.w = 0x41;
	for (int fr = 0; fr < 900; fr++) m.runFrame();
	for (auto &r : m.rows) printf("keyboard row %d read %d times\n", r.first, r.second);
	for (auto &j : m.joy) printf("PSG reg14 read with reg15=%02x : %d\n", j.first, j.second);
}
