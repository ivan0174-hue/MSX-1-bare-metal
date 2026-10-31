#include "vdp.h"
#include <string.h>

static const uint32_t palette[16] = {
	0xFF000000, 0xFF000000, 0xFF21C842, 0xFF5EDC78, 0xFF5455ED, 0xFF7D76FC, 0xFFD4524D, 0xFF42EBF5,
	0xFFFC5554, 0xFFFF7978, 0xFFD4C154, 0xFFE6CE80, 0xFF21B03B, 0xFFC95BBA, 0xFFCCCCCC, 0xFFFFFFFF
};

void Vdp::reset()
{
	memset(vram, 0, sizeof vram); memset(r, 0, sizeof r); memset(fb, 0, sizeof fb);
	status = 0; addr = 0; latch = readBuf = 0; latched = false;
}

void Vdp::writeData(uint8_t v) { latched = false; vram[addr] = v; readBuf = v; addr = (addr + 1) & 0x3FFF; }

uint8_t Vdp::readData()
{
	latched = false;
	uint8_t v = readBuf; readBuf = vram[addr]; addr = (addr + 1) & 0x3FFF;
	return v;
}

void Vdp::writeCtrl(uint8_t v)
{
	if (!latched) { latch = v; latched = true; return; }
	latched = false;
	if (v & 0x80) r[v & 7] = latch;
	else {
		addr = ((v & 0x3F) << 8) | latch;
		if (!(v & 0x40)) { readBuf = vram[addr]; addr = (addr + 1) & 0x3FFF; }
	}
}

uint8_t Vdp::readStatus() { latched = false; uint8_t s = status; status &= 0x1F; return s; }
void Vdp::setVBlank() { status |= 0x80; }

void Vdp::sprites(int y, uint8_t *line)
{
	int size = (r[1] & 2) ? 16 : 8, mag = r[1] & 1, h = size << mag;
	unsigned sat = (r[5] & 0x7F) << 7, spt = (r[6] & 7) << 11;
	int idx[4], n = 0;
	for (int s = 0; s < 32; s++) {
		int sy = vram[(sat + s * 4) & 0x3FFF];
		if (sy == 0xD0) break;
		if (sy >= 0xE0) sy -= 256;
		sy += 1;
		if (y < sy || y >= sy + h) continue;
		if (n == 4) { if (!(status & 0x40)) status = (status & 0xA0) | 0x40 | s; break; }
		idx[n++] = s;
	}
	uint8_t mask[256]; memset(mask, 0, sizeof mask);
	for (int k = n - 1; k >= 0; k--) {
		unsigned a = (sat + idx[k] * 4) & 0x3FFF;
		int sy = vram[a]; if (sy >= 0xE0) sy -= 256; sy += 1;
		int sx = vram[a + 1], pat = vram[a + 2], col = vram[a + 3] & 15;
		if (vram[a + 3] & 0x80) sx -= 32;
		int row = (y - sy) >> mag;
		for (int px = 0; px < h; px++) {
			int c = px >> mag, bit;
			if (size == 16) bit = (vram[(spt + (pat & 0xFC) * 8 + row + (c >= 8 ? 16 : 0)) & 0x3FFF] >> (7 - (c & 7))) & 1;
			else bit = (vram[(spt + pat * 8 + row) & 0x3FFF] >> (7 - c)) & 1;
			int x = sx + px;
			if (!bit || x < 0 || x > 255) continue;
			if (mask[x]) status |= 0x20;
			mask[x] = 1;
			if (col) line[x] = col;
		}
	}
}

void Vdp::renderLine(int y)
{
	uint8_t line[256];
	uint8_t bd = r[7] & 15;
	uint32_t *dst = fb + y * 256;
	if (!(r[1] & 0x40)) {
		for (int x = 0; x < 256; x++) dst[x] = palette[bd];
		return;
	}
	int row = y >> 3, ln = y & 7;
	unsigned nt = (r[2] & 0x0F) << 10;
	bool text = r[1] & 0x10, multi = r[1] & 0x08, g2 = r[0] & 0x02;
	memset(line, 0, sizeof line);

	if (text) {
		uint8_t fg = r[7] >> 4, bg = r[7] & 15;
		unsigned pt = (r[4] & 7) << 11;
		for (int x = 0; x < 8; x++) line[x] = line[248 + x] = 0;
		for (int c = 0; c < 40; c++) {
			uint8_t pat = vram[(pt + vram[(nt + row * 40 + c) & 0x3FFF] * 8 + ln) & 0x3FFF];
			for (int b = 0; b < 6; b++) line[8 + c * 6 + b] = (pat & (0x80 >> b)) ? fg : bg;
		}
	} else if (multi) {
		unsigned pt = (r[4] & 7) << 11;
		for (int c = 0; c < 32; c++) {
			uint8_t name = vram[(nt + row * 32 + c) & 0x3FFF];
			uint8_t v = vram[(pt + name * 8 + (row & 3) * 2 + (ln >> 2)) & 0x3FFF];
			for (int b = 0; b < 4; b++) { line[c * 8 + b] = v >> 4; line[c * 8 + 4 + b] = v & 15; }
		}
	} else if (g2) {
		int third = row >> 3;
		for (int c = 0; c < 32; c++) {
			unsigned name = vram[(nt + row * 32 + c) & 0x3FFF];
			unsigned pa = ((r[4] & 4) << 11) | ((third & (r[4] & 3)) << 11) | (name << 3) | ln;
			unsigned ca = ((r[3] & 0x80) << 6) | ((third & ((r[3] >> 5) & 3)) << 11) |
				      ((name & (((r[3] & 0x1F) << 3) | 7)) << 3) | ln;
			uint8_t pat = vram[pa & 0x3FFF], col = vram[ca & 0x3FFF];
			for (int b = 0; b < 8; b++) line[c * 8 + b] = (pat & (0x80 >> b)) ? (col >> 4) : (col & 15);
		}
	} else {
		unsigned ct = r[3] << 6, pt = (r[4] & 7) << 11;
		for (int c = 0; c < 32; c++) {
			uint8_t name = vram[(nt + row * 32 + c) & 0x3FFF];
			uint8_t pat = vram[(pt + name * 8 + ln) & 0x3FFF], col = vram[(ct + (name >> 3)) & 0x3FFF];
			for (int b = 0; b < 8; b++) line[c * 8 + b] = (pat & (0x80 >> b)) ? (col >> 4) : (col & 15);
		}
	}
	if (!text) sprites(y, line);
	for (int x = 0; x < 256; x++) dst[x] = palette[line[x] ? line[x] : bd];
}
