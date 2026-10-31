#include "z80.h"

enum { SF = 0x80, ZF = 0x40, YF = 0x20, HF = 0x10, XF = 0x08, PF = 0x04, NF = 0x02, CF = 0x01 };
#define A af.h
#define F af.l

static inline uint8_t par(uint8_t v) { v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return (v & 1) ? 0 : PF; }
static inline uint8_t szp(uint8_t v) { return (v & (SF | YF | XF)) | (v ? 0 : ZF) | par(v); }

void Z80::reset()
{
	af.w = bc.w = de.w = hl.w = ix.w = iy.w = sp.w = 0xFFFF;
	af2.w = bc2.w = de2.w = hl2.w = 0xFFFF;
	pc.w = 0; regI = regR = im = 0;
	iff1 = iff2 = halted = eiPending = useIdx = false;
	irq = false; t = 0; hx = &hl;
}

uint8_t Z80::fetch() { uint8_t v = rd(pc.w++); regR = (regR & 0x80) | ((regR + 1) & 0x7F); t += 5; return v; }
uint8_t Z80::imm8() { uint8_t v = rd(pc.w++); t += 3; return v; }
uint16_t Z80::imm16() { uint8_t l = imm8(); uint8_t h = imm8(); return l | (h << 8); }
uint8_t Z80::mr(uint16_t a) { t += 3; return rd(a); }
void Z80::mw(uint16_t a, uint8_t v) { t += 3; wr(a, v); }
uint16_t Z80::mr16(uint16_t a) { uint8_t l = mr(a); uint8_t h = mr(a + 1); return l | (h << 8); }
void Z80::mw16(uint16_t a, uint16_t v) { mw(a, v & 0xFF); mw(a + 1, v >> 8); }
void Z80::push(uint16_t v) { sp.w--; mw(sp.w, v >> 8); sp.w--; mw(sp.w, v & 0xFF); }
uint16_t Z80::pop() { uint8_t l = mr(sp.w++); uint8_t h = mr(sp.w++); return l | (h << 8); }

uint8_t Z80::getR(int z, bool x)
{
	switch (z) {
	case 0: return bc.h; case 1: return bc.l; case 2: return de.h; case 3: return de.l;
	case 4: return (x && useIdx) ? hx->h : hl.h;
	case 5: return (x && useIdx) ? hx->l : hl.l;
	default: return A;
	}
}

void Z80::setR(int z, uint8_t v, bool x)
{
	switch (z) {
	case 0: bc.h = v; break; case 1: bc.l = v; break; case 2: de.h = v; break; case 3: de.l = v; break;
	case 4: if (x && useIdx) hx->h = v; else hl.h = v; break;
	case 5: if (x && useIdx) hx->l = v; else hl.l = v; break;
	default: A = v; break;
	}
}

R16 &Z80::rp(int p) { switch (p) { case 0: return bc; case 1: return de; case 2: return *hx; default: return sp; } }
R16 &Z80::rp2(int p) { switch (p) { case 0: return bc; case 1: return de; case 2: return *hx; default: return af; } }

bool Z80::cc(int i)
{
	static const uint8_t mask[4] = { ZF, CF, PF, SF };
	bool f = (F & mask[i >> 1]) != 0;
	return (i & 1) ? f : !f;
}

uint16_t Z80::ea()
{
	if (!useIdx) return hl.w;
	int8_t d = (int8_t)imm8(); t += 5;
	return hx->w + d;
}

void Z80::alu(int op, uint8_t v)
{
	int a = A, r;
	switch (op) {
	case 0: case 1: {
		int c = (op == 1) ? (F & CF) : 0;
		r = a + v + c;
		F = (r & (SF | YF | XF)) | ((r & 0xFF) ? 0 : ZF) | ((a ^ v ^ r) & HF) |
		    (((a ^ r) & (v ^ r) & 0x80) ? PF : 0) | ((r >> 8) & 1);
		A = r; break; }
	case 2: case 3: case 7: {
		int c = (op == 3) ? (F & CF) : 0;
		r = a - v - c;
		int yx = (op == 7) ? (v & (YF | XF)) : (r & (YF | XF));
		F = (r & SF) | yx | ((r & 0xFF) ? 0 : ZF) | ((a ^ v ^ r) & HF) |
		    (((a ^ v) & (a ^ r) & 0x80) ? PF : 0) | NF | ((r & 0x100) ? CF : 0);
		if (op != 7) { A = r; } break; }
	case 4: A &= v; F = szp(A) | HF; break;
	case 5: A ^= v; F = szp(A); break;
	default: A |= v; F = szp(A); break;
	}
}

uint8_t Z80::inc8(uint8_t v)
{
	uint8_t r = v + 1;
	F = (F & CF) | (r & (SF | YF | XF)) | (r ? 0 : ZF) | (((v & 0xF) == 0xF) ? HF : 0) | (v == 0x7F ? PF : 0);
	return r;
}

uint8_t Z80::dec8(uint8_t v)
{
	uint8_t r = v - 1;
	F = (F & CF) | (r & (SF | YF | XF)) | (r ? 0 : ZF) | (((v & 0xF) == 0) ? HF : 0) | (v == 0x80 ? PF : 0) | NF;
	return r;
}

uint16_t Z80::add16(uint16_t a, uint16_t b)
{
	uint32_t r = (uint32_t)a + b;
	F = (F & (SF | ZF | PF)) | ((r >> 8) & (YF | XF)) | (((a ^ b ^ r) >> 8) & HF) | ((r >> 16) & 1);
	return r;
}

uint16_t Z80::adc16(uint16_t a, uint16_t b)
{
	uint32_t r = (uint32_t)a + b + (F & CF);
	F = ((r >> 8) & (SF | YF | XF)) | ((r & 0xFFFF) ? 0 : ZF) | (((a ^ b ^ r) >> 8) & HF) |
	    ((((a ^ r) & (b ^ r)) & 0x8000) ? PF : 0) | ((r >> 16) & 1);
	return r;
}

uint16_t Z80::sbc16(uint16_t a, uint16_t b)
{
	uint32_t r = (uint32_t)a - b - (F & CF);
	F = ((r >> 8) & (SF | YF | XF)) | ((r & 0xFFFF) ? 0 : ZF) | (((a ^ b ^ r) >> 8) & HF) |
	    ((((a ^ b) & (a ^ r)) & 0x8000) ? PF : 0) | NF | ((r & 0x10000) ? CF : 0);
	return r;
}

uint8_t Z80::rot(int y, uint8_t v)
{
	uint8_t c;
	switch (y) {
	case 0: c = v >> 7; v = (v << 1) | c; break;
	case 1: c = v & 1; v = (v >> 1) | (c << 7); break;
	case 2: c = v >> 7; v = (v << 1) | (F & CF); break;
	case 3: c = v & 1; v = (v >> 1) | ((F & CF) << 7); break;
	case 4: c = v >> 7; v = v << 1; break;
	case 5: c = v & 1; v = (v >> 1) | (v & 0x80); break;
	case 6: c = v >> 7; v = (v << 1) | 1; break;
	default: c = v & 1; v = v >> 1; break;
	}
	F = szp(v) | c;
	return v;
}

void Z80::daa()
{
	int a = A, diff = 0, c = F & CF, hf;
	if ((F & HF) || (a & 0xF) > 9) diff |= 6;
	if (c || a > 0x99) { diff |= 0x60; c = CF; }
	if (F & NF) { hf = ((F & HF) && (a & 0xF) < 6) ? HF : 0; a -= diff; }
	else        { hf = ((a & 0xF) > 9) ? HF : 0; a += diff; }
	A = a;
	F = szp(A) | (F & NF) | hf | c;
}

void Z80::execCB(uint8_t op, bool idx, uint16_t addr)
{
	int x = op >> 6, y = (op >> 3) & 7, z = op & 7;
	bool mem = idx || z == 6;
	uint8_t v;
	if (mem) { if (!idx) addr = hl.w; v = mr(addr); } else v = getR(z, false);
	switch (x) {
	case 0: v = rot(y, v); break;
	case 1: {
		uint8_t m = v & (1 << y);
		uint8_t src = idx ? (addr >> 8) : v;
		F = (F & CF) | HF | (m ? (m & SF) : (ZF | PF)) | (src & (YF | XF));
		return; }
	case 2: v &= ~(1 << y); break;
	default: v |= (1 << y); break;
	}
	if (mem) { t += 1; mw(addr, v); if (idx && z != 6) setR(z, v, false); }
	else setR(z, v, false);
}

void Z80::block(int y, int z)
{
	bool dec = y & 1, rep = y >= 6;
	int d = dec ? -1 : 1;
	switch (z) {
	case 0: {
		uint8_t v = mr(hl.w); mw(de.w, v);
		hl.w += d; de.w += d; bc.w--;
		uint8_t n = A + v;
		F = (F & (SF | ZF | CF)) | ((n & 2) ? YF : 0) | (n & XF) | (bc.w ? PF : 0);
		t += 2;
		if (rep && bc.w) { pc.w -= 2; t += 5; }
		break; }
	case 1: {
		uint8_t v = mr(hl.w);
		uint8_t r = A - v;
		uint8_t hf = (A ^ v ^ r) & HF;
		hl.w += d; bc.w--;
		uint8_t n = r - (hf ? 1 : 0);
		F = (F & CF) | NF | (r & SF) | (r ? 0 : ZF) | hf | ((n & 2) ? YF : 0) | (n & XF) | (bc.w ? PF : 0);
		t += 5;
		if (rep && bc.w && r) { pc.w -= 2; t += 5; }
		break; }
	case 2: {
		t += 1;
		uint8_t v = in(bc.w); mw(hl.w, v);
		hl.w += d; bc.h--;
		F = (bc.h ? 0 : ZF) | NF | (bc.h & (SF | YF | XF));
		if (rep && bc.h) { pc.w -= 2; t += 5; }
		break; }
	default: {
		t += 1;
		uint8_t v = mr(hl.w);
		bc.h--; out(bc.w, v);
		hl.w += d;
		F = (bc.h ? 0 : ZF) | NF | (bc.h & (SF | YF | XF));
		if (rep && bc.h) { pc.w -= 2; t += 5; }
		break; }
	}
}

void Z80::execED()
{
	uint8_t op = fetch();
	int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1;
	R16 *rpe[4] = { &bc, &de, &hl, &sp };
	if (x == 1) {
		switch (z) {
		case 0: { uint8_t v = in(bc.w); t += 4; F = (F & CF) | szp(v); if (y != 6) setR(y, v, false); break; }
		case 1: { out(bc.w, y == 6 ? 0 : getR(y, false)); t += 4; break; }
		case 2: t += 7;
			if (y & 1) hl.w = adc16(hl.w, rpe[p]->w); else hl.w = sbc16(hl.w, rpe[p]->w);
			break;
		case 3: { uint16_t nn = imm16();
			if (y & 1) rpe[p]->w = mr16(nn); else mw16(nn, rpe[p]->w);
			break; }
		case 4: { uint8_t v = A; A = 0; alu(2, v); break; }
		case 5: pc.w = pop(); iff1 = iff2; break;
		case 6: { int m = y & 3; im = (m == 2) ? 1 : (m == 3) ? 2 : 0; break; }
		default:
			switch (y) {
			case 0: regI = A; t += 1; break;
			case 1: regR = A; t += 1; break;
			case 2: A = regI; F = (F & CF) | szp(A) | 0; F = (F & ~PF) | (iff2 ? PF : 0); t += 1; break;
			case 3: A = regR; F = (F & CF) | szp(A) | 0; F = (F & ~PF) | (iff2 ? PF : 0); t += 1; break;
			case 4: { uint8_t v = mr(hl.w), lo = A & 0xF;
				A = (A & 0xF0) | (v & 0xF); v = (v >> 4) | (lo << 4); t += 4; mw(hl.w, v);
				F = (F & CF) | szp(A); break; }
			case 5: { uint8_t v = mr(hl.w), lo = A & 0xF;
				A = (A & 0xF0) | (v >> 4); v = (v << 4) | lo; t += 4; mw(hl.w, v);
				F = (F & CF) | szp(A); break; }
			default: break;
			}
		}
	} else if (x == 2 && z < 4 && y >= 4) {
		block(y, z);
	}
	// everything else behaves as NOP
}

int Z80::step()
{
	t = 0;
	if (irq && iff1 && !eiPending) {
		halted = false; iff1 = iff2 = false;
		regR = (regR & 0x80) | ((regR + 1) & 0x7F);
		push(pc.w);
		if (im == 2) { uint16_t a = (regI << 8) | 0xFF; pc.w = mr16(a); }
		else pc.w = 0x38;
		t += 7;
		return t;
	}
	eiPending = false;
	if (halted) { regR = (regR & 0x80) | ((regR + 1) & 0x7F); t = 5; return t; }

	hx = &hl; useIdx = false;
	uint8_t op;
	for (;;) {
		op = fetch();
		if (op == 0xDD) { hx = &ix; useIdx = true; }
		else if (op == 0xFD) { hx = &iy; useIdx = true; }
		else break;
	}
	if (op == 0xED) { useIdx = false; execED(); return t; }
	if (op == 0xCB) {
		if (useIdx) { int8_t d = (int8_t)imm8(); uint8_t o2 = imm8(); t += 2; execCB(o2, true, hx->w + d); }
		else { uint8_t o2 = fetch(); execCB(o2, false, 0); }
		return t;
	}

	int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1;
	switch (x) {
	case 0:
		switch (z) {
		case 0:
			switch (y) {
			case 0: break;
			case 1: { uint16_t tmp = af.w; af.w = af2.w; af2.w = tmp; break; }
			case 2: { int8_t d = (int8_t)imm8(); t += 1; if (--bc.h) { pc.w += d; t += 5; } break; }
			case 3: { int8_t d = (int8_t)imm8(); pc.w += d; t += 5; break; }
			default: { int8_t d = (int8_t)imm8(); if (cc(y - 4)) { pc.w += d; t += 5; } break; }
			}
			break;
		case 1:
			if (!(y & 1)) rp(p).w = imm16();
			else { hx->w = add16(hx->w, rp(p).w); t += 7; }
			break;
		case 2:
			switch (y) {
			case 0: mw(bc.w, A); break;
			case 1: A = mr(bc.w); break;
			case 2: mw(de.w, A); break;
			case 3: A = mr(de.w); break;
			case 4: { uint16_t nn = imm16(); mw16(nn, hx->w); break; }
			case 5: { uint16_t nn = imm16(); hx->w = mr16(nn); break; }
			case 6: { uint16_t nn = imm16(); mw(nn, A); break; }
			default: { uint16_t nn = imm16(); A = mr(nn); break; }
			}
			break;
		case 3:
			if (!(y & 1)) rp(p).w++; else rp(p).w--;
			t += 2;
			break;
		case 4:
			if (y == 6) { uint16_t a = ea(); uint8_t v = mr(a); v = inc8(v); t += 1; mw(a, v); }
			else setR(y, inc8(getR(y, true)), true);
			break;
		case 5:
			if (y == 6) { uint16_t a = ea(); uint8_t v = mr(a); v = dec8(v); t += 1; mw(a, v); }
			else setR(y, dec8(getR(y, true)), true);
			break;
		case 6:
			if (y == 6) { uint16_t a = ea(); uint8_t n = imm8(); mw(a, n); }
			else setR(y, imm8(), true);
			break;
		default:
			switch (y) {
			case 0: { uint8_t c = A >> 7; A = (A << 1) | c; F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | c; break; }
			case 1: { uint8_t c = A & 1; A = (A >> 1) | (c << 7); F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | c; break; }
			case 2: { uint8_t c = A >> 7; A = (A << 1) | (F & CF); F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | c; break; }
			case 3: { uint8_t c = A & 1; A = (A >> 1) | ((F & CF) << 7); F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | c; break; }
			case 4: daa(); break;
			case 5: A = ~A; F = (F & (SF | ZF | PF | CF)) | HF | NF | (A & (YF | XF)); break;
			case 6: F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | CF; break;
			default: { uint8_t oc = F & CF; F = (F & (SF | ZF | PF)) | (A & (YF | XF)) | (oc ? HF : 0) | (oc ? 0 : CF); break; }
			}
		}
		break;

	case 1:
		if (y == 6 && z == 6) halted = true;
		else if (z == 6) { uint16_t a = ea(); setR(y, mr(a), false); }
		else if (y == 6) { uint16_t a = ea(); mw(a, getR(z, false)); }
		else setR(y, getR(z, true), true);
		break;

	case 2:
		if (z == 6) { uint16_t a = ea(); alu(y, mr(a)); }
		else alu(y, getR(z, true));
		break;

	default:
		switch (z) {
		case 0: t += 1; if (cc(y)) pc.w = pop(); break;
		case 1:
			if (!(y & 1)) rp2(p).w = pop();
			else switch (p) {
				case 0: pc.w = pop(); break;
				case 1: { uint16_t a = bc.w, b = de.w, c = hl.w;
					bc.w = bc2.w; de.w = de2.w; hl.w = hl2.w; bc2.w = a; de2.w = b; hl2.w = c; break; }
				case 2: pc.w = hx->w; break;
				default: sp.w = hx->w; t += 2; break;
			}
			break;
		case 2: { uint16_t nn = imm16(); if (cc(y)) pc.w = nn; break; }
		case 3:
			switch (y) {
			case 0: pc.w = imm16(); break;
			case 2: { uint8_t n = imm8(); out((A << 8) | n, A); t += 4; break; }
			case 3: { uint8_t n = imm8(); A = in((A << 8) | n); t += 4; break; }
			case 4: { uint16_t v = mr16(sp.w); mw16(sp.w, hx->w); hx->w = v; t += 3; break; }
			case 5: { uint16_t tmp = de.w; de.w = hl.w; hl.w = tmp; break; }
			case 6: iff1 = iff2 = false; break;
			case 7: iff1 = iff2 = true; eiPending = true; break;
			default: break;
			}
			break;
		case 4: { uint16_t nn = imm16(); if (cc(y)) { t += 1; push(pc.w); pc.w = nn; } break; }
		case 5:
			if (!(y & 1)) { t += 1; push(rp2(p).w); }
			else { uint16_t nn = imm16(); t += 1; push(pc.w); pc.w = nn; }
			break;
		case 6: alu(y, imm8()); break;
		default: t += 1; push(pc.w); pc.w = y * 8; break;
		}
	}
	return t;
}
