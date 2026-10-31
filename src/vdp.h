// TMS9918A video display processor (MSX1): text, graphic 1/2, multicolor, sprites
#ifndef VDP_H
#define VDP_H
#include <stdint.h>

class Vdp
{
public:
	Vdp() { reset(); }
	void reset();
	void writeData(uint8_t v);
	uint8_t readData();
	void writeCtrl(uint8_t v);
	uint8_t readStatus();
	void setVBlank();
	bool irq() const { return (status & 0x80) && (r[1] & 0x20); }
	void renderLine(int y);
	const uint32_t *frame() const { return fb; }

	uint8_t vram[0x4000];
	uint8_t r[8];
	uint8_t status;
private:
	uint16_t addr;
	uint8_t latch, readBuf;
	bool latched;
	uint32_t fb[256 * 192];
	void sprites(int y, uint8_t *line);
};

#endif
