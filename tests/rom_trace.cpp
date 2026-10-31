#include <stdlib.h>
#include "msx.h"
#include <stdio.h>
int main(int argc, char **argv)
{
	static uint8_t rom[0x10000]; FILE *f = fopen(argv[1], "rb"); size_t n = fread(rom, 1, sizeof rom, f);
	Msx m; m.setBios(rom, n); m.hardReset(); m.ppiA = (int)strtol(argv[2], 0, 16);
	int N = atoi(argv[3]);
	for (int i = 0; i < N; i++) { unsigned pc = m.pc.w; printf("%04x sp=%04x a=%02x bc=%04x hl=%04x : %02x %02x %02x\n", pc, m.sp.w, m.af.h, m.bc.w, m.hl.w, rom[pc & 0x7FFF], rom[(pc+1) & 0x7FFF], rom[(pc+2)&0x7FFF]); m.step(); }
}
