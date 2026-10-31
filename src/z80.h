// Z80 CPU core - original implementation (decode by x/y/z bit fields)
#ifndef Z80_H
#define Z80_H
#include <stdint.h>

// little-endian host assumed (x86, ARM)
union R16 { uint16_t w; struct { uint8_t l, h; }; };

class Z80
{
public:
	Z80() { reset(); }
	virtual ~Z80() {}

	void reset();
	int  step();			// one instruction or interrupt; returns T-states (MSX: +1 wait per M1)

	bool irq;			// level-triggered INT line (set by the machine)

	R16 af, bc, de, hl, ix, iy, sp, pc, af2, bc2, de2, hl2;
	uint8_t regI, regR, im;
	bool iff1, iff2, halted;

protected:
	virtual uint8_t rd(uint16_t a) = 0;
	virtual void    wr(uint16_t a, uint8_t v) = 0;
	virtual uint8_t in(uint16_t port) = 0;
	virtual void    out(uint16_t port, uint8_t v) = 0;
	int t;				// T-states of the instruction being executed

private:
	bool eiPending, useIdx;
	R16 *hx;

	uint8_t  fetch();
	uint8_t  imm8();
	uint16_t imm16();
	uint8_t  mr(uint16_t a);
	void     mw(uint16_t a, uint8_t v);
	uint16_t mr16(uint16_t a);
	void     mw16(uint16_t a, uint16_t v);
	void     push(uint16_t v);
	uint16_t pop();
	uint8_t  getR(int z, bool x);
	void     setR(int z, uint8_t v, bool x);
	R16     &rp(int p);
	R16     &rp2(int p);
	bool     cc(int i);
	uint16_t ea();
	void     alu(int op, uint8_t v);
	uint8_t  inc8(uint8_t v);
	uint8_t  dec8(uint8_t v);
	uint16_t add16(uint16_t a, uint16_t b);
	uint16_t adc16(uint16_t a, uint16_t b);
	uint16_t sbc16(uint16_t a, uint16_t b);
	uint8_t  rot(int y, uint8_t v);
	void     daa();
	void     execCB(uint8_t op, bool idx, uint16_t addr);
	void     execED();
	void     block(int y, int z);
};

#endif
