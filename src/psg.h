// AY-3-8910 PSG. render() produces mono samples; audio output is not wired in phase 1.
#ifndef PSG_H
#define PSG_H
#include <stdint.h>

class Psg
{
public:
	Psg() : nEv(0), logging(false) { reset(); }
	void reset();
	void selectReg(uint8_t v) { latch = v & 15; }
	void write(uint8_t v, uint32_t cycle = 0);	// cycle: CPU cycles since the frame start (used when logging)
	uint8_t read() const { return reg[latch]; }
	uint8_t selected() const { return latch; }
	void render(int16_t *out, int n, int rate);	// immediate mode: sound follows reg[] as is
	// Logged mode (used by the machine): register writes carry a timestamp and are applied at the right
	// sample while the frame is rendered. frameCycles = length of the frame in CPU cycles.
	void renderFrame(int16_t *out, int n, int rate, uint32_t frameCycles);
	void setLogging(bool on) { logging = on; nEv = 0; }
	uint8_t reg[16];
private:
	struct Ev { uint32_t cycle; uint8_t reg, val; };
	enum { MAXEV = 8192 };
	Ev ev[MAXEV]; int nEv; bool logging;
	uint8_t sreg[16];			// register values seen by the sound generator
	void apply(uint8_t r, uint8_t v);
	void mix(int16_t *out, int n, int rate, uint32_t frameCycles, bool timed);
	uint8_t latch;
	int cnt[3], lvl[3];
	int noiseCnt; uint32_t lfsr; int noiseOut;
	int envCnt, envVol, envDir; bool envHold, attack;
	double acc;
	void tick();
	void envReset();
};

#endif
