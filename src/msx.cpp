#include "msx.h"
#include <string.h>

Msx::Msx() : ppiA(0), ppiC(0), cycles(0), frameStart(0), frameCycles(59736), budget(0)
{
	memset(ram, 0, sizeof ram); memset(bios, 0xFF, sizeof bios); memset(keys, 0, sizeof keys);
	disk = 0; diskSize = 0; dbgHook = 0; sysRom = false; sysInit = 0; memset(biosSaved, 0xFF, sizeof biosSaved);
}

void Msx::hardReset()
{
	Z80::reset();
	memset(ram, 0, sizeof ram); memset(keys, 0, sizeof keys);
	ppiA = 0; ppiC = 0; budget = 0;
	vdp.reset(); psg.reset(); tape.setMotor(false, cycles);
	if (sysRom) { ppiA = 0xF0; pc.w = sysInit; }		// ROM at 0000h-7FFFh, RAM in pages 2-3, start at INIT
}

bool Msx::isSystemRom(const uint8_t *d, size_t n, uint16_t *init)
{
	if (!d || (n != 0x4000 && n != 0x8000) || d[0] != 'A' || d[1] != 'B') return false;
	unsigned i = d[2] | (d[3] << 8);
	if (i == 0 || i >= 0x4000) return false;
	if (init) *init = (uint16_t)i;
	return true;
}

void Msx::ejectCart()
{
	cart.eject();
	if (sysRom) { memcpy(bios, biosSaved, sizeof bios); sysRom = false; }
}

void Msx::setCart(const uint8_t *d, size_t n, MapperType t)
{
	uint16_t init;
	if (isSystemRom(d, n, &init)) {
		if (!sysRom) memcpy(biosSaved, bios, sizeof bios);
		memcpy(bios, d, n); if (n == 0x4000) memcpy(bios + 0x4000, d, 0x4000);	// 16 KB image: mirrored
		cart.eject(); sysRom = true; sysInit = init;
		hardReset();
		return;
	}
	if (sysRom) { memcpy(bios, biosSaved, sizeof bios); sysRom = false; }
	cart.load(d, n, t);
}

const uint8_t *Msx::charset() const
{
	const uint8_t *b = sysRom ? biosSaved : bios;		// the menu font always comes from the real BIOS
	unsigned p = b[4] | (b[5] << 8);
	return (p >= 8 && p + 2048 <= sizeof bios) ? b + p : 0;
}

bool Msx::setBios(const uint8_t *d, size_t n)
{
	if (n < 0x4000) return false;
	memset(bios, 0xFF, sizeof bios);
	memcpy(bios, d, n > sizeof bios ? sizeof bios : n);
	return true;
}

uint8_t Msx::rd(uint16_t a)
{
	switch (slotOf(a)) {
	case 0: return a < 0x8000 ? bios[a] : 0xFF;
	case 1: return cart.read(a);
	case 3: return ram[a];
	default: return 0xFF;
	}
}

void Msx::wr(uint16_t a, uint8_t v)
{
	switch (slotOf(a)) {
	case 1: cart.write(a, v); break;
	case 3: ram[a] = v; break;
	default: break;
	}
}

uint8_t Msx::in(uint16_t port)
{
	switch (port & 0xFF) {
	case 0x98: return vdp.readData();
	case 0x99: return vdp.readStatus();
	case 0xA2:
		if (psg.selected() == 14) {		// joystick (none) + cassette input on bit 7
			uint8_t v = 0x7F;
			if (tape.level(cycles + t)) v |= 0x80;
			return v;
		}
		return psg.read();
	case 0xA8: return ppiA;
	case 0xA9: { int row = ppiC & 15; return row <= 10 ? (uint8_t)~keys[row] : 0xFF; }
	case 0xAA: return ppiC;
	default: return 0xFF;
	}
}

void Msx::out(uint16_t port, uint8_t v)
{
	switch (port & 0xFF) {
	case 0x98: vdp.writeData(v); break;
	case 0x99: vdp.writeCtrl(v); break;
	case 0xA0: psg.selectReg(v); break;
	case 0xA1: psg.write(v, (uint32_t)(cycles + t - frameStart)); break;
	case 0xA8: ppiA = v; break;
	case 0xAA: ppiC = v; tape.setMotor(!(ppiC & 0x10), cycles + t); break;
	case 0xAB:
		if (!(v & 0x80)) {
			int b = (v >> 1) & 7;
			if (v & 1) ppiC |= 1 << b; else ppiC &= ~(1 << b);
			tape.setMotor(!(ppiC & 0x10), cycles + t);
		}
		break;
	default: break;
	}
}

void Msx::runFrame()
{
	frameStart = cycles;
	for (int line = 0; line < 262; line++) {
		if (dbgHook && (line & 15) == 0) dbgHook(line, pc.w);
		if (line == 192) vdp.setVBlank();
		budget += 228;
		while (budget > 0) {
			irq = vdp.irq();
			int c = step();
			budget -= c; cycles += c;
		}
		if (line < 192) vdp.renderLine(line);
	}
	frameCycles = (uint32_t)(cycles - frameStart);
}
