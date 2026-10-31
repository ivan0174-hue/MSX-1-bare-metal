#include <stdlib.h>
#include "msx.h"
#include <stdio.h>
#include <string.h>
static void shot(Msx &m, const char *fn) {
	FILE *o = fopen(fn, "wb"); fprintf(o, "P6 256 192 255\n"); const uint32_t *fb = m.vdp.frame();
	for (int i = 0; i < 256*192; i++) { fputc((fb[i]>>16)&255,o); fputc((fb[i]>>8)&255,o); fputc(fb[i]&255,o);} fclose(o);
}
int main(int argc, char **argv)
{
	static uint8_t rom[0x10000]; FILE *f = fopen(argv[1], "rb"); size_t n = fread(rom, 1, sizeof rom, f);
	int row = atoi(argv[2]), bit = atoi(argv[3]); const char *out = argv[4]; int joy = argc > 5;
	Msx m; m.setBios(rom, n); m.hardReset(); m.ppiA = 0xF0; m.pc.w = 0x41;
	uint8_t k[11] = {0};
	for (int fr = 0; fr < 600; fr++) {
		if (fr == 300) { if (row >= 0) k[row] |= 1 << bit; m.setKeys(k); }
		if (fr == 330) { memset(k, 0, sizeof k); m.setKeys(k); }
		m.runFrame();
	}
	shot(m, out);
	printf("ram C000: "); for (int i = 0; i < 16; i++) printf("%02x ", m.ram[0xC000 + i]); printf("\n");
}
