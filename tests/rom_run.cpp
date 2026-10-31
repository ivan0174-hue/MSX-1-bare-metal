#include <stdlib.h>
#include "msx.h"
#include <stdio.h>
#include <string.h>
#include <map>
struct M : Msx {
	std::map<int,int> ports;
	uint8_t in(uint16_t p) { ports[0x100 | (p & 255)]++; return Msx::in(p); }
	void out(uint16_t p, uint8_t v) { ports[p & 255]++; Msx::out(p, v); }
};
int main(int argc, char **argv)
{
	static uint8_t rom[0x10000]; FILE *f = fopen(argv[1], "rb"); size_t n = fread(rom, 1, sizeof rom, f);
	int ppi = argc > 2 ? (int)strtol(argv[2], 0, 16) : 0xC0; int frames = argc > 3 ? atoi(argv[3]) : 300;
	M m; m.setBios(rom, n); m.hardReset(); m.ppiA = ppi; if (argc > 4) { m.pc.w = (uint16_t)strtol(argv[4], 0, 16); }
	for (int fr = 0; fr < frames; fr++) { m.runFrame(); if (fr % 60 == 0) printf("frame %d pc=%04x sp=%04x iff=%d halted=%d ppiA=%02x\n", fr, m.pc.w, m.sp.w, m.iff1, m.halted, m.ppiA); }
	FILE *o = fopen("/tmp/rom.ppm", "wb"); fprintf(o, "P6 256 192 255\n"); const uint32_t *fb = m.vdp.frame(); for (int i = 0; i < 256*192; i++) { fputc((fb[i]>>16)&255,o); fputc((fb[i]>>8)&255,o); fputc(fb[i]&255,o);} fclose(o);
	for (auto &p : m.ports) printf("port %s%02x : %d\n", p.first & 0x100 ? "IN " : "OUT ", p.first & 255, p.second);
}
