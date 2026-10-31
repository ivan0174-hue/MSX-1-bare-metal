#include "psg.h"
#include <string.h>

void Psg::reset()
{
	memset(reg, 0, sizeof reg); reg[7] = 0xFF; memcpy(sreg, reg, sizeof sreg); nEv = 0;
	latch = 0; for (int i = 0; i < 3; i++) { cnt[i] = 0; lvl[i] = 0; }
	noiseCnt = 0; lfsr = 1; noiseOut = 0; envCnt = 0; envVol = 0; envDir = 1; envHold = true; attack = false; acc = 0;
}

void Psg::envReset()
{
	attack = (sreg[13] & 4) != 0;
	envVol = attack ? 0 : 15; envDir = attack ? 1 : -1; envHold = false; envCnt = 0;
}

// Apply a register value to the sound generator (shadow registers).
void Psg::apply(uint8_t r, uint8_t v)
{
	sreg[r] = v;
	if (r == 13) { sreg[13] &= 15; envReset(); }
}

void Psg::write(uint8_t v, uint32_t cycle)
{
	reg[latch] = v;
	if (!logging) { apply(latch, v); return; }
	if (nEv < MAXEV) { ev[nEv].cycle = cycle; ev[nEv].reg = latch; ev[nEv].val = v; nEv++; }
	else apply(latch, v);			// overflow: lose timing, keep the sound right
}

void Psg::tick()
{
	for (int c = 0; c < 3; c++) {
		int p = sreg[c * 2] | ((sreg[c * 2 + 1] & 15) << 8); if (p < 1) p = 1;
		if (++cnt[c] >= p) { cnt[c] = 0; lvl[c] ^= 1; }
	}
	int np = sreg[6] & 31; if (np < 1) np = 1;
	if (++noiseCnt >= np) {
		noiseCnt = 0;
		uint32_t bit = (lfsr ^ (lfsr >> 3)) & 1;
		lfsr = (lfsr >> 1) | (bit << 16);
		noiseOut = lfsr & 1;
	}
	int ep = sreg[11] | (sreg[12] << 8); if (ep < 1) ep = 1;
	if (!envHold && ++envCnt >= ep) {
		envCnt = 0; envVol += envDir;
		if (envVol < 0 || envVol > 15) {
			int s = sreg[13], L = envVol > 15 ? 15 : 0;
			if (!(s & 8)) { envVol = 0; envHold = true; }
			else if (s & 1) { envHold = true; envVol = (s & 2) ? 15 - L : L; }
			else if (s & 2) { envDir = -envDir; envVol = L; }
			else envVol = attack ? 0 : 15;
		}
	}
}

// amplitude per volume step: 0.7071^(15-v), v=0 is silence (precomputed: no libm needed)
static const double kAmp[16] = { 0.00000, 0.00781, 0.01105, 0.01562, 0.02209, 0.03125, 0.04419, 0.06250, 0.08838, 0.12499, 0.17677, 0.24999, 0.35354, 0.49999, 0.70710, 1.00000 };

void Psg::mix(int16_t *out, int n, int rate, uint32_t frameCycles, bool timed)
{
	const double step = 111860.8 / rate;
	int e = 0;
	for (int i = 0; i < n; i++) {
		if (timed) {
			uint32_t t = (uint32_t)((uint64_t)i * frameCycles / n);
			while (e < nEv && ev[e].cycle <= t) { apply(ev[e].reg, ev[e].val); e++; }
		}
		acc += step;
		while (acc >= 1.0) { acc -= 1.0; tick(); }
		double s = 0;
		for (int c = 0; c < 3; c++) {
			bool tone = ((sreg[7] >> c) & 1) ? true : lvl[c];
			bool nz = ((sreg[7] >> (c + 3)) & 1) ? true : noiseOut;
			if (!(tone && nz)) continue;
			int v = sreg[8 + c];
			int a = (v & 16) ? envVol : (v & 15);
			s += kAmp[a & 15];
		}
		out[i] = (int16_t)(s * 7000);
	}
	if (timed) { for (; e < nEv; e++) apply(ev[e].reg, ev[e].val); nEv = 0; }
}

void Psg::render(int16_t *out, int n, int rate)
{
	memcpy(sreg, reg, sizeof sreg);		// immediate mode: follow reg[] directly
	sreg[13] &= 15;
	mix(out, n, rate, 0, false);
}

void Psg::renderFrame(int16_t *out, int n, int rate, uint32_t frameCycles)
{
	mix(out, n, rate, frameCycles ? frameCycles : 1, true);
}
