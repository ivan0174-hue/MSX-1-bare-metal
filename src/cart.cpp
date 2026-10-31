#include "cart.h"

MapperType Cart::detect(const uint8_t *d, size_t n)
{
	if (n <= 0x8000) return M_PLAIN;
	unsigned c5000 = 0, c9000 = 0, cB000 = 0, c8000 = 0, cA000 = 0, c6800 = 0, c7800 = 0, c77FF = 0, c6000 = 0, c7000 = 0;
	for (size_t i = 0; i + 2 < n; i++) {
		if (d[i] != 0x32) continue;			// LD (nnnn),A
		unsigned a = d[i + 1] | (d[i + 2] << 8);
		switch (a) {
		case 0x5000: c5000++; break; case 0x9000: c9000++; break; case 0xB000: cB000++; break;
		case 0x8000: c8000++; break; case 0xA000: cA000++; break; case 0x6800: c6800++; break;
		case 0x7800: c7800++; break; case 0x77FF: c77FF++; break; case 0x6000: c6000++; break;
		case 0x7000: c7000++; break; default: break;
		}
	}
	unsigned konami = c8000 + cA000, scc = c5000 + c9000 + cB000, a8 = c6800 + c7800, a16 = c77FF;
	unsigned best = konami; MapperType t = M_KONAMI;
	if (scc > best) { best = scc; t = M_KONAMI_SCC; }
	if (a8 > best) { best = a8; t = M_ASCII8; }
	if (a16 > best) { best = a16; t = M_ASCII16; }
	if (best == 0) t = (c6000 + c7000) ? M_ASCII16 : M_ASCII8;
	return t;
}

const char *Cart::name(MapperType t)
{
	switch (t) {
	case M_PLAIN: return "plain"; case M_KONAMI: return "konami"; case M_KONAMI_SCC: return "konami-scc";
	case M_ASCII8: return "ascii8"; case M_ASCII16: return "ascii16"; default: return "auto";
	}
}

void Cart::load(const uint8_t *data, size_t n, MapperType t)
{
	rom = data; size = n;
	type = (t == M_AUTO) ? detect(data, n) : t;
	nb8 = (n + 0x1FFF) >> 13; if (!nb8) nb8 = 1;
	nb16 = (n + 0x3FFF) >> 14; if (!nb16) nb16 = 1;
	base = 0x4000;
	if (type == M_PLAIN && n <= 0x4000 && n >= 4 && data[0] == 'A' && data[1] == 'B' && (data[3] >= 0x80)) base = 0x8000;
	bank[0] = 0; bank[1] = 1; bank[2] = 2; bank[3] = 3;
	if (type == M_ASCII8 || type == M_ASCII16) bank[0] = bank[1] = bank[2] = bank[3] = 0;
	if (type == M_ASCII16) bank[1] = 0;
}

uint8_t Cart::read(uint16_t a) const
{
	if (!rom || a < 0x4000 || a >= 0xC000) return 0xFF;
	size_t off;
	switch (type) {
	case M_PLAIN:
		if (a < base) return 0xFF;
		off = a - base; return off < size ? rom[off] : 0xFF;
	case M_ASCII16:
		off = (size_t)(bank[(a >> 15) & 1] % nb16) * 0x4000 + (a & 0x3FFF);
		return off < size ? rom[off] : 0xFF;
	default:
		off = (size_t)(bank[(a - 0x4000) >> 13] % nb8) * 0x2000 + (a & 0x1FFF);
		return off < size ? rom[off] : 0xFF;
	}
}

void Cart::write(uint16_t a, uint8_t v)
{
	if (!rom) return;
	switch (type) {
	case M_KONAMI:
		if (a >= 0x6000 && a < 0xC000) bank[(a - 0x4000) >> 13] = v;
		break;
	case M_KONAMI_SCC:
		if ((a & 0xF800) == 0x5000) bank[0] = v & 0x3F;
		else if ((a & 0xF800) == 0x7000) bank[1] = v & 0x3F;
		else if ((a & 0xF800) == 0x9000) bank[2] = v & 0x3F;
		else if ((a & 0xF800) == 0xB000) bank[3] = v & 0x3F;
		break;
	case M_ASCII8:
		if (a >= 0x6000 && a < 0x8000) bank[(a >> 11) & 3] = v;
		break;
	case M_ASCII16:
		if (a >= 0x6000 && a < 0x6800) bank[0] = v;
		else if (a >= 0x7000 && a < 0x7800) bank[1] = v;
		break;
	default: break;
	}
}
