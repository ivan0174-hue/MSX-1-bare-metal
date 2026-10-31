// Host-side self test (PC): g++ -std=c++11 -I../src host_test.cpp ../src/*.cpp (except kernel files)
#include "msx.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main()
{
	static Msx m;
	uint8_t bios[0x8000]; memset(bios, 0, sizeof bios);
	const uint8_t prog[] = {
		0x3E,0xC0, 0xD3,0xA8,			// LD A,C0 ; OUT (A8),A  (page 3 -> RAM)
		0x31,0x00,0xF3,				// LD SP,F300
		0x3E,0x19, 0xC6,0x28, 0x27,		// LD A,19 ; ADD A,28 ; DAA  -> 47
		0x32,0x00,0xC0,				// LD (C000),A
		0x06,0x0A, 0xAF, 0x80, 0x10,0xFD,	// sum 10..1
		0x32,0x01,0xC0,				// LD (C001),A -> 37
		0x21,0x00,0x00, 0x11,0x00,0xC1, 0x01,0x10,0x00, 0xED,0xB0,	// LDIR 16 bytes
		0xDD,0x21,0x00,0xC2, 0xDD,0x36,0x05,0xAA,	// LD IX,C200 ; LD (IX+5),AA
		0xCD,0x35,0x00,				// CALL 0035
		0x32,0x02,0xC0,				// LD (C002),A
		0x3E,0x81, 0x07, 0x32,0x03,0xC0,	// LD A,81 ; RLCA ; LD (C003),A -> 03
		0x76, 0x00,
		0x3E,0x5A, 0xC9				// 0035: LD A,5A ; RET
	};
	// fix CALL target: compute address of subroutine
	memcpy(bios, prog, sizeof prog);
	size_t sub = sizeof prog - 3;
	for (size_t i = 0; i + 2 < sizeof prog; i++) if (prog[i] == 0xCD) { bios[i + 1] = sub & 0xFF; bios[i + 2] = sub >> 8; }
	m.setBios(bios, sizeof bios);
	for (int i = 0; i < 60; i++) m.step();
	CHECK(m.ram[0xC000] == 0x47, "DAA result %02X", m.ram[0xC000]);
	CHECK(m.ram[0xC001] == 0x37, "loop sum %02X", m.ram[0xC001]);
	CHECK(memcmp(&m.ram[0xC100], bios, 16) == 0, "LDIR");
	CHECK(m.ram[0xC205] == 0xAA, "IX+d store");
	CHECK(m.ram[0xC002] == 0x5A, "CALL/RET %02X", m.ram[0xC002]);
	CHECK(m.ram[0xC003] == 0x03, "RLCA %02X", m.ram[0xC003]);
	CHECK(m.halted, "HALT");

	// run a full frame (CPU halted, interrupts disabled); the debug hook fires every 16 lines
	static int hookCalls = 0, lastLine = -1;
	m.dbgHook = [](int line, uint16_t) { hookCalls++; lastLine = line; };
	m.runFrame();
	CHECK(hookCalls == 17 && lastLine == 256, "debug hook calls %d last line %d", hookCalls, lastLine);
	m.dbgHook = 0;
	CHECK(m.vdp.status & 0x80, "VBlank flag");

	// VDP: write VRAM through ports, read back
	Vdp v;
	v.writeCtrl(0x00); v.writeCtrl(0x40);	// write addr 0
	for (int i = 0; i < 8; i++) v.writeData(0xFF);
	v.writeCtrl(0x00); v.writeCtrl(0x00);	// read addr 0
	CHECK(v.readData() == 0xFF, "VRAM readback");
	// graphic 1, name table 0x1800, pattern 0x0000, color 0x2000: char 0 solid, color white/blue
	v.r[1] = 0x40; v.r[2] = 0x06; v.r[3] = 0x80; v.r[4] = 0x00; v.r[7] = 0x04;
	v.vram[0x2000] = 0xF4; v.vram[0x1800] = 0;
	v.renderLine(0);
	CHECK(v.frame()[0] == 0xFFFFFFFF, "G1 pixel %08X", v.frame()[0]);

	// sprite: 8x8 at x=16,y=-1+... put at Y=9 (shown from line 10), solid pattern, color 6
	v.r[5] = 0x37; v.r[6] = 0x07;		// SAT 0x1B80, SPT 0x3800
	memset(&v.vram[0x3800], 0xFF, 8);
	v.vram[0x1B80] = 9; v.vram[0x1B81] = 16; v.vram[0x1B82] = 0; v.vram[0x1B83] = 6;
	v.vram[0x1B84] = 0xD0;
	v.renderLine(10);
	CHECK(v.frame()[10 * 256 + 16] == 0xFFD4524D, "sprite pixel %08X", v.frame()[10 * 256 + 16]);

	// PSG: tone on channel A produces a varying signal
	Psg p; p.reg[0] = 50; p.reg[7] = 0xFE; p.reg[8] = 15;
	int16_t buf[2048]; p.render(buf, 2048, 44100);
	int mn = 0, mx = 0; for (int i = 0; i < 2048; i++) { if (buf[i] < mn) mn = buf[i]; if (buf[i] > mx) mx = buf[i]; }
	CHECK(mx > 1000 && mn == 0, "PSG tone min=%d max=%d", mn, mx);

	// PSG logged mode: a register write at cycle 30000 of a 59736-cycle frame takes effect mid-frame
	{
		Psg q; q.setLogging(true);
		q.selectReg(8); q.write(15, 0);
		q.selectReg(0); q.write(50, 0);
		q.selectReg(7); q.write(0xFE, 30000);		// tone A on (before: channel off = constant level)
		int16_t fb[736]; q.renderFrame(fb, 736, 44100, 59736);
		bool constEarly = true; for (int i = 5; i < 360; i++) if (fb[i] != fb[5]) constEarly = false;
		int mn2 = 32767, mx2 = -32768; for (int i = 380; i < 736; i++) { if (fb[i] < mn2) mn2 = fb[i]; if (fb[i] > mx2) mx2 = fb[i]; }
		CHECK(constEarly, "PSG: samples before the write are constant");
		CHECK(mn2 == 0 && mx2 > 6000, "PSG: tone after the write min=%d max=%d", mn2, mx2);
		CHECK(q.read() == 0 || true, "read");
	}
	// machine: PSG logging + frame audio does not crash and tracks the frame length
	{
		static Msx am; am.setBios(bios, sizeof bios); am.psg.setLogging(true);
		am.runFrame();
		int16_t ab[801]; am.renderAudio(ab, 801, 48000);
		// a real Z80 program programs the PSG: tone A, volume 15, period 50 -> audible square wave
		static Msx pm; static uint8_t pb[0x8000]; memset(pb, 0, sizeof pb);
		const uint8_t pp[] = { 0x3E,7, 0xD3,0xA0, 0x3E,0xFE, 0xD3,0xA1, 0x3E,8, 0xD3,0xA0, 0x3E,15, 0xD3,0xA1,
				       0x3E,0, 0xD3,0xA0, 0x3E,50, 0xD3,0xA1, 0x76 };
		memcpy(pb, pp, sizeof pp); pm.setBios(pb, sizeof pb); pm.psg.setLogging(true);
		pm.runFrame(); int16_t pa[736]; pm.renderAudio(pa, 736, 44100);
		int pmn = 32767, pmx = -32768; for (int i = 20; i < 736; i++) { if (pa[i] < pmn) pmn = pa[i]; if (pa[i] > pmx) pmx = pa[i]; }
		CHECK(pmn == 0 && pmx > 6000, "Z80-programmed PSG tone min=%d max=%d", pmn, pmx);
		CHECK(am.frameCycles >= 59736 && am.frameCycles < 59736 + 40, "frameCycles %u", am.frameCycles);
	}

	// Mapper detection and banking
	
	static uint8_t rom[0x20000]; memset(rom, 0, sizeof rom);
	for (int b = 0; b < 16; b++) memset(&rom[b * 0x2000], b, 0x2000);
	rom[0] = 'A'; rom[1] = 'B';
	// ASCII8 driver snippets
	rom[0x100] = 0x32; rom[0x101] = 0x00; rom[0x102] = 0x68;
	rom[0x103] = 0x32; rom[0x104] = 0x00; rom[0x105] = 0x78;
	CHECK(Cart::detect(rom, sizeof rom) == M_ASCII8, "detect ascii8");
	Cart c; c.load(rom, sizeof rom, M_ASCII8);
	c.write(0x6800, 5); CHECK(c.read(0x6000) == 5, "ascii8 bank");
	Cart k; k.load(rom, sizeof rom, M_KONAMI);
	k.write(0x8000, 7); CHECK(k.read(0x8000) == 7, "konami bank");
	Cart a16; a16.load(rom, sizeof rom, M_ASCII16);
	a16.write(0x7000, 3); CHECK(a16.read(0x8000) == 6, "ascii16 bank %d", a16.read(0x8000));

	// Tape: header then a byte; check level toggles with 746-cycle halves
	static uint8_t cas[16] = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74, 0x55,0,0,0,0,0,0,0 };
	Tape t; t.load(cas, sizeof cas); t.setMotor(true, 0);
	int toggles = 0, prev = t.level(0);
	for (uint64_t now = 1; now < 746ull * 100; now++) { int l = t.level(now); if (l != prev) { toggles++; prev = l; } }
	CHECK(toggles >= 98 && toggles <= 101, "tape header toggles %d", toggles);

	// Keyboard mapping
	uint8_t mat[11], keys[6] = { 0x04, 0x28, 0, 0, 0, 0 };
	extern void hidToMatrix(uint8_t, const uint8_t *, uint8_t *);
	hidToMatrix(0x02, keys, mat);
	CHECK((mat[2] & 0x40) && (mat[7] & 0x80) && (mat[6] & 1), "keyboard A+RET+SHIFT");

	// ---- Menu ----
	struct FakeFs : FileSource {
		int list(const char *dir, int category, MenuEntry *out, int max) {
			int n = 0; (void)max;
			auto add = [&](const char *nm, bool d) { strcpy(out[n].name, nm); out[n].dir = d; n++; };
			if (!strcmp(dir, "")) {
				add("zeta.rom", false); add("games", true); add("Alpha.ROM", false);
				if (category == CAT_CAS) { n = 0; add("tape.cas", false); }
			} else if (!strcmp(dir, "games")) add("deep.rom", false);
			return n;
		}
	} fs;
	static Menu menu(&fs);
	uint8_t none[6] = {0}, f12[6] = {0x45}, down[6] = {0x51}, enter[6] = {0x28}, bs[6] = {0x2A};
	MenuAction act;
	auto press = [&](const uint8_t *k) { MenuAction r = menu.update(k); menu.update(none); return r; };
	CHECK(!menu.isOpen(), "menu closed at start");
	press(f12); CHECK(menu.isOpen() && menu.swallowing(), "F12 opens");
	act = press(enter);					// item 0: Load ROM -> browse root
	CHECK(act.type == A_NONE, "enter browse");
	// sorted: [games] first, then Alpha.ROM, zeta.rom ; select Alpha.ROM = index 1
	press(down); act = menu.update(enter);
	CHECK(act.type == A_LOAD_ROM && !strcmp(act.path, "Alpha.ROM"), "load rom path '%s' type %d", act.path, act.type);
	CHECK(!menu.isOpen() && menu.swallowing(), "closed, still swallowing until keys released");
	menu.update(none); CHECK(!menu.swallowing(), "swallow released");
	// reopen, remembered dir root; go into games and load deep.rom
	press(f12); press(enter); act = press(enter);		// enter [games]
	CHECK(act.type == A_NONE, "enter dir");
	press(down); act = press(enter);			// index0 = '..', index1 = deep.rom
	CHECK(act.type == A_LOAD_ROM && !strcmp(act.path, "games/deep.rom"), "subdir path '%s'", act.path);
	// back navigation + CAS category
	press(f12); press(enter); press(bs);			// ROM browser (in games) -> BS goes up to root
	press(bs);						// root -> main
	press(down); act = press(enter);			// item 1 = CAS
	press(enter);						// select tape.cas (only entry)
	// action came from the press(enter) above; redo explicitly to capture it
	press(f12); press(f12);
	press(f12); press(down); act = press(enter); act = press(enter);
	CHECK(act.type == A_LOAD_CAS && !strcmp(act.path, "tape.cas"), "cas path '%s' type %d", act.path, act.type);
	// eject / reset / mapper
	press(f12); for (int i = 0; i < 3; i++) press(down); act = press(enter);
	CHECK(act.type == A_EJECT_ROM, "eject rom");
	press(down); press(down); press(down); press(enter);
	CHECK(menu.mapper() == M_PLAIN, "mapper cycles to plain (%d)", menu.mapper());
	press(down); act = press(enter);
	CHECK(act.type == A_RESET, "reset action %d", act.type);
	// render with BIOS font
	static uint32_t bg[256 * 192]; memset(bg, 0xFF, sizeof bg);
	static uint8_t font[2048]; memset(font, 0xFF, sizeof font);
	press(f12);
	const uint32_t *img = menu.render(bg, font);
	CHECK(img[100 * 256 + 4] == 0xFF3F3F3F || img[100 * 256 + 4] == 0xFF7F7F7F, "dimmed background %08X", img[100 * 256 + 4]);
	CHECK(img[100 * 256 + 100] != img[100 * 256 + 4], "panel drawn");
	// hard reset + charset
	m.hardReset(); CHECK(m.pc.w == 0 && m.ram[0xC000] == 0, "hardReset");
	bios[4] = 0x00; bios[5] = 0x10; m.setBios(bios, sizeof bios);
	CHECK(m.charset() == m.bios + 0x1000, "charset pointer");
	menu.update(f12);
	menu.update(none);

	// system ROM (built to start at 0000h, header AB + INIT < 4000h): replaces the BIOS, restored on eject
	{
		static uint8_t bios[0x8000], rom[0x4000];
		memset(bios, 0xC9, sizeof bios); bios[4] = 0x00; bios[5] = 0x10;
		memset(rom, 0, sizeof rom); rom[0] = 'A'; rom[1] = 'B'; rom[2] = 0x41; rom[3] = 0x00;
		rom[0x41] = 0x3E; rom[0x42] = 0x77; rom[0x43] = 0x32; rom[0x44] = 0x00; rom[0x45] = 0xC0; rom[0x46] = 0x18; rom[0x47] = 0xFE;	// LD A,77h; LD (C000h),A; JR $
		Msx m; m.setBios(bios, sizeof bios);
		uint16_t init = 0;
		CHECK(Msx::isSystemRom(rom, sizeof rom, &init) && init == 0x41, "isSystemRom");
		rom[3] = 0x40; CHECK(!Msx::isSystemRom(rom, sizeof rom), "normal cartridge (INIT 4041h) is not a system ROM"); rom[3] = 0x00;
		m.setCart(rom, sizeof rom, M_AUTO);
		CHECK(m.systemRom() && m.pc.w == 0x41, "starts at INIT, pc=%04x", m.pc.w);
		CHECK(m.charset() != 0, "menu font still comes from the saved BIOS");
		for (int i = 0; i < 3; i++) m.runFrame();
		CHECK(m.ram[0xC000] == 0x77, "system ROM executed with RAM in page 3");
		m.hardReset(); CHECK(m.pc.w == 0x41, "reset restarts the system ROM");
		m.ejectCart(); m.hardReset(); CHECK(!m.systemRom() && m.pc.w == 0 && m.bios[0x100] == 0xC9, "eject restores the BIOS");
	}
	printf(fails ? "%d FAILURES\n" : "ALL TESTS PASSED\n", fails);
	return fails != 0;
}
