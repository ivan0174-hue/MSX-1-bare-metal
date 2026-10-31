// Play a real .cas through Tape and decode the FSK like the BIOS: compare every block with the file.
#include "tape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
int main(int argc, char **argv)
{
	FILE *f = fopen(argv[1], "rb"); std::vector<uint8_t> cas(1 << 20); size_t n = fread(cas.data(), 1, cas.size(), f); cas.resize(n);
	bool fast = argc > 2;
	Tape t; t.load(cas.data(), n); if (fast) t.setFast(true); t.rewind();
	t.setMotor(true, 0);
	std::vector<int> halves; int prev = t.level(0); uint64_t last = 0;
	for (uint64_t now = 1; now < 3579545ull * 400 && !t.finished(); now++) { int l = t.level(now); if (l != prev) { halves.push_back((int)(now - last)); last = now; prev = l; } }
	printf("halves %zu finished %d percent %u\n", halves.size(), (int)t.finished(), t.percent());
	size_t i = 0; int bn = 0; size_t total = 0;
	while (i < halves.size()) {
		size_t j = i; while (j < halves.size() && halves[j] < 1100 && abs(halves[j] - halves[i]) < 30) j++;
		if (j - i < 1000) { i++; continue; }
		int u = halves[i]; i = j; std::vector<uint8_t> out;
		for (;;) {
			size_t k = i; while (k < halves.size() && abs(halves[k] - halves[i]) < 30) k++;
			if (k - i >= 1000) break;
			if (i + 1 >= halves.size()) break;
			if (!(halves[i] > 1.5 * u && halves[i + 1] > 1.5 * u)) { i++; continue; }
			i += 2; uint8_t b = 0; bool ok = true;
			for (int bit = 0; bit < 8 && ok; bit++) {
				if (i + 1 < halves.size() && halves[i] > 1.5 * u && halves[i + 1] > 1.5 * u) i += 2;
				else if (i + 3 < halves.size() && halves[i] < 1.5 * u && halves[i + 3] < 1.5 * u) { b |= 1 << bit; i += 4; }
				else ok = false;
			}
			if (!ok) break;
			i += 8; out.push_back(b);
		}
		printf("block %d unit %d bytes %zu\n", bn++, u, out.size()); total += out.size();
	}
	printf("total decoded %zu (file %zu minus headers 24)\n", total, n);
}
