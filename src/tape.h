// .CAS tape playback: turns the byte stream into 1200 baud FSK (0 = 1 cycle 1200 Hz, 1 = 2 cycles 2400 Hz)
#ifndef TAPE_H
#define TAPE_H
#include <stdint.h>
#include <stddef.h>

class Tape
{
public:
	Tape() : data(0), size(0), motorOn(false), last(0), lastValid(false), fastReq(false), curFast(false) { rewind(); }
	void load(const uint8_t *d, size_t n) { data = d; size = n; rewind(); }
	void rewind();
	void eject() { data = 0; size = 0; }
	bool present() const { return data != 0; }
	bool active() const { return data != 0 && motorOn; }
	bool finished() const { return data != 0 && ended; }	// whole tape played, nothing more to read	// tape inserted and motor running
	unsigned percent() const		// position of the block being played, 0..100
	{
		if (!data || !size) return 0;
		if (ended) return 100;
		size_t p = (headerHalves > 0 && pos >= 8) ? pos - 8 : pos;	// the header is not data yet
		return (unsigned)((uint64_t)p * 100 / size);
	}
	void setFast(bool f) { fastReq = f; }			// 2x speed, takes effect at the next block header
	bool fast() const { return fastReq; }
	bool playingFast() const { return curFast; }		// speed of the block being played right now
	void setMotor(bool on, uint64_t now) { sync(now); motorOn = on; }
	int level(uint64_t now) { sync(now); return lvl; }
private:
	const uint8_t *data; size_t size; bool motorOn; uint64_t last; bool lastValid, fastReq, curFast;
	int lvl; int64_t remaining;
	size_t pos, blockEnd; int headerHalves, bitIdx, bitHalves, curLen; uint8_t curByte; bool ended;
	void sync(uint64_t now);
	int nextHalf();
	int nextHalfBase();
	size_t findHeader(size_t from) const;
	bool isHeader(size_t p) const;
};

#endif
