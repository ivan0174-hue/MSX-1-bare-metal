// MSX1 machine: slot 0 = BIOS (32K), slot 1 = cartridge, slot 3 = 64K RAM
#ifndef MSX_H
#define MSX_H
#include "z80.h"
#include "vdp.h"
#include "psg.h"
#include "cart.h"
#include "tape.h"

class Msx : public Z80
{
public:
	Msx();
	bool setBios(const uint8_t *d, size_t n);
	void setCart(const uint8_t *d, size_t n, MapperType t);	// also detects "system" ROMs (see isSystemRom)
	void ejectCart();
	bool systemRom() const { return sysRom; }
	// ROM images built to start at 0000h (header "AB" with INIT in 0001h..3FFFh, 16/32 KB): they replace the BIOS
	static bool isSystemRom(const uint8_t *d, size_t n, uint16_t *init = 0);
	void setCas(const uint8_t *d, size_t n) { tape.load(d, n); }
	void hardReset();				// power-cycle: CPU, RAM, VDP, PSG, PPI
	void setDisk(const uint8_t *d, size_t n) { disk = d; diskSize = n; }	// stored only; WD2793 comes in phase 2
	const uint8_t *charset() const;			// MSX font from the BIOS (CGTABL at 0004h) or null
	void runFrame();
	// Audio for the frame just emulated (call after runFrame): n samples mono at 'rate' Hz
	void renderAudio(int16_t *out, int n, int rate) { psg.renderFrame(out, n, rate, frameCycles); }				// 262 scanlines x 228 cycles
	void setKeys(const uint8_t m[11]) { for (int i = 0; i < 11; i++) keys[i] = m[i]; }

	Vdp vdp; Psg psg; Cart cart; Tape tape;
	uint8_t ram[0x10000];
	uint8_t bios[0x8000];
	uint8_t ppiA, ppiC;
	uint64_t cycles;
	uint64_t frameStart; uint32_t frameCycles;	// cycle count at the start of the last frame, and its length
	typedef void (*DbgHook)(int line, uint16_t pc);
	DbgHook dbgHook;				// optional: diagnostics, called every 16 scanlines
	const uint8_t *disk; size_t diskSize;
	bool sysRom; uint16_t sysInit; uint8_t biosSaved[0x8000];	// system ROM mode: the real BIOS is kept here

protected:
	uint8_t rd(uint16_t a);
	void wr(uint16_t a, uint8_t v);
	uint8_t in(uint16_t port);
	void out(uint16_t port, uint8_t v);
private:
	uint8_t keys[11];
	int budget;
	int slotOf(uint16_t a) const { return (ppiA >> ((a >> 14) * 2)) & 3; }
};

#endif
