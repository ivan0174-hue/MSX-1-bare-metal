#include "tape.h"

static const uint8_t HDR[8] = { 0x1F, 0xA6, 0xDE, 0xBA, 0xCC, 0x13, 0x7D, 0x74 };
enum { HALF_2400 = 746, HALF_1200 = 1492, HEADER_HALVES = 16000 };

bool Tape::isHeader(size_t p) const
{
	if (p + 8 > size) return false;
	for (int i = 0; i < 8; i++) if (data[p + i] != HDR[i]) return false;
	return true;
}

size_t Tape::findHeader(size_t from) const
{
	// Official .cas files keep headers 8-byte aligned, but many converted/edited files do not:
	// look at every byte offset so no block is merged into the previous one.
	size_t p = from;
	while (p + 8 <= size) { if (isHeader(p)) return p; p++; }
	return size;
}

void Tape::rewind()
{
	pos = 0; blockEnd = 0; headerHalves = 0; bitIdx = 11; bitHalves = 0; curLen = HALF_1200; curByte = 0; ended = false;
	lvl = 0; remaining = 0; last = 0; lastValid = false; curFast = fastReq;	// time reference is re-taken at the next sync
	if (data) remaining = nextHalf();
}

int Tape::nextHalf()
{
	int h = nextHalfBase();
	return (curFast && h < 1000000) ? h / 2 : h;
}

int Tape::nextHalfBase()
{
	for (;;) {
		if (headerHalves > 0) { headerHalves--; return HALF_2400; }
		if (bitHalves > 0) { bitHalves--; return curLen; }
		if (bitIdx > 10) {
			if (pos >= size) { ended = true; return 3579545; }
			if (pos == blockEnd) {
				if (isHeader(pos)) pos += 8;
				blockEnd = findHeader(pos);
				headerHalves = HEADER_HALVES;
				curFast = fastReq;		// BIOS measures speed from the header: change only here
				continue;
			}
			curByte = data[pos++]; bitIdx = 0;
		}
		int bit = (bitIdx == 0) ? 0 : (bitIdx <= 8) ? ((curByte >> (bitIdx - 1)) & 1) : 1;
		bitIdx++;
		if (bit) { curLen = HALF_2400; bitHalves = 3; } else { curLen = HALF_1200; bitHalves = 1; }
		return curLen;
	}
}

void Tape::sync(uint64_t now)
{
	if (!lastValid) { last = now; lastValid = true; return; }	// first call after load/rewind: no time elapsed
	int64_t d = (int64_t)(now - last); last = now;
	if (!motorOn || !data || d <= 0) return;
	remaining -= d;
	while (remaining <= 0) { lvl ^= 1; remaining += nextHalf(); }
}
