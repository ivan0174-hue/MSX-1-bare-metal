// Decode the FSK signal produced by Tape (like the MSX BIOS would): 1x, and 2x applied at a block boundary.
#include "tape.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <stdlib.h>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main()
{
	const uint8_t H[8] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
	std::vector<uint8_t> cas;
	const char *p[3] = { "HELLO", "WORLD!!", "THIRD" };
	for (int b = 0; b < 3; b++) {
		cas.insert(cas.end(), H, H + 8); cas.insert(cas.end(), p[b], p[b] + strlen(p[b]));
		while (cas.size() % 8) cas.push_back(0);
	}
	Tape t; t.load(cas.data(), cas.size());
	CHECK(t.percent() == 0 && !t.active(), "initial state");
	t.setMotor(true, 0);
	CHECK(t.active(), "active with motor");

	// run, request fast shortly after the start (while block 1 is playing), watch progress
	std::vector<int> halves; int prev = t.level(0); uint64_t last = 0, end = 3579545ull * 14;
	unsigned lastPct = 0; bool monotonic = true, askedFast = false, fast1 = false, fastAfter = false;
	for (uint64_t now = 1; now < end; now++) {
		if (!askedFast && now == 3579545ull / 2) { t.setFast(true); askedFast = true; }
		int l = t.level(now);
		if (l != prev) { halves.push_back((int)(now - last)); last = now; prev = l; }
		if ((now & 0xFFFF) == 0) { unsigned pc = t.percent(); if (pc < lastPct) monotonic = false; lastPct = pc; }
		if (now == 1000000) fast1 = t.playingFast();		// first block must still be normal speed
		if (now == end - 1) fastAfter = t.playingFast();
	}
	CHECK(monotonic, "percent monotonic");
	CHECK(!fast1, "speed must not change in the middle of block 1");
	CHECK(fastAfter, "later blocks play fast");
	CHECK(t.percent() == 100, "percent at end %u", lastPct);

	// decode block by block; unit = header half period
	size_t i = 0; std::vector<std::vector<uint8_t>> blocks; std::vector<int> units;
	while (i < halves.size()) {
		size_t j = i; while (j < halves.size() && halves[j] < 1100 && abs(halves[j] - halves[i]) < 30) j++;
		if (j - i < 1000) { i++; continue; }
		int u = halves[i]; units.push_back(u); i = j;
		std::vector<uint8_t> out;
		for (;;) {
			// next header? (run of units)
			size_t k = i; while (k < halves.size() && abs(halves[k] - halves[i]) < 30) k++;
			if (k - i >= 1000) break;
			if (i + 1 >= halves.size()) break;
			if (!(halves[i] > 1.5 * u && halves[i + 1] > 1.5 * u)) { i++; continue; }	// start bit: two long halves
			i += 2; uint8_t b = 0; bool ok = true;
			for (int bit = 0; bit < 8 && ok; bit++) {
				if (i + 1 < halves.size() && halves[i] > 1.5 * u && halves[i + 1] > 1.5 * u) i += 2;
				else if (i + 3 < halves.size() && halves[i] < 1.5 * u && halves[i+3] < 1.5 * u) { b |= 1 << bit; i += 4; }
				else ok = false;
			}
			if (!ok) break;
			i += 8; out.push_back(b);
		}
		blocks.push_back(out);
	}
	CHECK(blocks.size() == 3, "blocks decoded: %zu", blocks.size());
	if (blocks.size() == 3) {
		CHECK(units[0] > 700 && units[0] < 800, "block1 unit %d", units[0]);
		CHECK(units[1] > 350 && units[1] < 400, "block2 unit %d (fast)", units[1]);
		CHECK(units[2] > 350 && units[2] < 400, "block3 unit %d (fast)", units[2]);
		CHECK(!memcmp(blocks[0].data(), "HELLO", 5), "block1 data");
		CHECK(!memcmp(blocks[1].data(), "WORLD!!", 7), "block2 data");
		CHECK(!memcmp(blocks[2].data(), "THIRD", 5), "block3 data");
	}
	// reload while the motor is on and the machine has been running for a long time: no fast-forward
	{
		Tape r; r.load(cas.data(), cas.size()); r.setMotor(true, 5000000000ull);
		r.level(5000000000ull + 100000);
		CHECK(r.percent() == 0 && !r.finished(), "reload must not skip: percent %u finished %d", r.percent(), (int)r.finished());
		r.load(cas.data(), cas.size());				// reload with the motor still on (menu while loading)
		r.level(9000000000ull + 1000);
		CHECK(r.percent() == 0 && !r.finished(), "reload with motor on must not skip: percent %u", r.percent());
		CHECK(t.finished(), "finished() after the whole tape was played");
	}
	// headers NOT aligned to 8 bytes (edited/converted .cas files): every block must still get its own header
	{
		std::vector<uint8_t> u; const char *q[3] = { "ONE", "TWO22", "THREE" };
		for (int b = 0; b < 3; b++) { u.insert(u.end(), H, H + 8); u.insert(u.end(), q[b], q[b] + strlen(q[b])); if (b == 1) u.push_back(0); }
		Tape r; r.load(u.data(), u.size()); r.setMotor(true, 0);
		int hdr = 0, run = 0, prevH = 0; int pl = r.level(0); uint64_t lastT = 0;
		for (uint64_t now = 1; now < 3579545ull * 12 && !r.finished(); now++) {
			int l = r.level(now);
			if (l != pl) { int h = (int)(now - lastT); lastT = now; pl = l; if (h > 700 && h < 800) { if (++run == 1000) hdr++; } else run = 0; }
		}
		(void)prevH;
		CHECK(hdr == 3, "unaligned headers: %d header runs, expected 3", hdr);
	}
	printf(fails ? "%d FAILURES\n" : "TAPE 1x/2x DECODE OK (units %d / %d / %d)\n", fails ? fails : units[0], units.size() > 1 ? units[1] : 0, units.size() > 2 ? units[2] : 0);
	return fails != 0;
}
