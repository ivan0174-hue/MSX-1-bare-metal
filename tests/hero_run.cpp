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
	static uint8_t cas[1 << 20]; FILE *f = fopen(argv[1], "rb"); size_t n = fread(cas, 1, sizeof cas, f);
	M m; uint8_t bios[0x8000]; memset(bios, 0xC9, sizeof bios);
	const uint8_t isr[] = { 0xF5, 0xDB, 0x99, 0xF1, 0xCD, 0x9A, 0xFD, 0xFB, 0xC9 }; memcpy(bios + 0x38, isr, sizeof isr);
	bios[0] = 0xF3; bios[1] = 0xC3; bios[2] = 0x00; bios[3] = 0x40; // not used
	m.setBios(bios, sizeof bios); m.hardReset();
	memcpy(m.ram + 0x8000, cas + 2128, 16896);
	m.ppiA = 0xF0; m.pc.w = 0x8000; m.sp.w = 0xF380;
	// FD9A default hook = RET
	m.ram[0xFD9A] = 0xC9;
	for (int fr = 0; fr < 600; fr++) {
		m.runFrame();
		if (fr % 100 == 0) printf("frame %d pc=%04x sp=%04x iff=%d halted=%d ppiA=%02x\n", fr, m.pc.w, m.sp.w, m.iff1, m.halted, m.ppiA);
	}
	{ FILE *o = fopen("/tmp/hero.ppm", "wb"); fprintf(o, "P6 256 192 255\n"); const uint32_t *fb = m.vdp.frame(); for (int i = 0; i < 256*192; i++) { fputc((fb[i]>>16)&255,o); fputc((fb[i]>>8)&255,o); fputc(fb[i]&255,o);} fclose(o); }
	for (auto &p : m.ports) printf("port %s%02x : %d\n", p.first & 0x100 ? "IN " : "OUT ", p.first & 255, p.second);
}
