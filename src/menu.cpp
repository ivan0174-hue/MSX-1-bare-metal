#include "menu.h"
#include <string.h>
#include "mstr.h"

enum { K_F12 = 0x45, K_UP = 0x52, K_DOWN = 0x51, K_ENTER = 0x28, K_KPENTER = 0x58, K_ESC = 0x29,
       K_PGUP = 0x4B, K_PGDN = 0x4E, K_BS = 0x2A, K_HOME = 0x4A, K_END = 0x4D };
static const uint8_t kKeys[11] = { K_F12, K_UP, K_DOWN, K_ENTER, K_KPENTER, K_ESC, K_PGUP, K_PGDN, K_BS, K_HOME, K_END };

static const char *mainItems[9] = {
	"Load ROM cartridge...", "Load tape (CAS)...", "Load disk (DSK)...",
	"Eject cartridge", "Eject tape", "Eject disk", "", "Reset MSX", "Close menu"
};
static const char *catNames[3] = { "ROM", "CAS", "DSK" };

static int ci(const char *a, const char *b)
{
	for (;;) {
		char x = *a++, y = *b++;
		if (x >= 'a' && x <= 'z') x -= 32;
		if (y >= 'a' && y <= 'z') y -= 32;
		if (x != y) return x < y ? -1 : 1;
		if (!x) return 0;
	}
}

static void copyStr(char *d, size_t n, const char *s) { m_strncpy(d, s, n - 1); d[n - 1] = 0; }

Menu::Menu(FileSource *s) : src(s), open(false), swallow(false), page(P_MAIN), sel(0), top(0), cat(0), first(0), count(0), mapperIdx(0)
{
	dir[0] = 0; msg[0] = 0;
	for (int i = 0; i < 3; i++) { lastDir[i][0] = 0; copyStr(loaded[i], sizeof loaded[i], "-"); }
	for (int i = 0; i < 11; i++) { was[i] = false; hold[i] = 0; }
	memset(fb, 0, sizeof fb);
}

bool Menu::matches(int category, const char *name)
{
	const char *dot = m_strrchr(name, '.');
	if (!dot) return false;
	dot++;
	switch (category) {
	case CAT_ROM: return !ci(dot, "rom") || !ci(dot, "mx1");
	case CAT_CAS: return !ci(dot, "cas");
	default: return !ci(dot, "dsk");
	}
}

void Menu::setLoaded(int c, const char *name) { copyStr(loaded[c], sizeof loaded[c], name && *name ? name : "-"); }
void Menu::setMessage(const char *m) { copyStr(msg, sizeof msg, m); }

MenuAction Menu::update(const uint8_t keys[6])
{
	MenuAction a; a.type = A_NONE; a.path[0] = 0;
	bool any = false;
	for (int k = 0; k < 6; k++) if (keys[k]) any = true;
	for (int i = 0; i < 11; i++) {
		bool now = false;
		for (int k = 0; k < 6; k++) if (keys[k] == kKeys[i]) now = true;
		bool ev = false;
		if (now) {
			if (!was[i]) { ev = true; hold[i] = 0; }
			else {
				hold[i]++;
				bool rep = (kKeys[i] == K_UP || kKeys[i] == K_DOWN || kKeys[i] == K_PGUP || kKeys[i] == K_PGDN);
				if (rep && hold[i] >= 20 && ((hold[i] - 20) % 4) == 0) ev = true;
			}
		}
		was[i] = now;
		if (ev && a.type == A_NONE) handle(kKeys[i], a);
	}
	if (open) swallow = true;
	else if (swallow && !any) swallow = false;
	return a;
}

void Menu::handle(uint8_t key, MenuAction &a)
{
	if (key == K_F12) {
		if (open) open = false;
		else { open = true; swallow = true; page = P_MAIN; sel = 0; top = 0; msg[0] = 0; }
		return;
	}
	if (!open) return;
	if (page == P_MAIN) mainKey(key, a); else browseKey(key, a);
}

void Menu::fixScroll()
{
	if (sel < top) top = sel;
	if (sel >= top + ROWS) top = sel - ROWS + 1;
	if (top < 0) top = 0;
}

void Menu::mainKey(uint8_t key, MenuAction &a)
{
	const int N = 9;
	switch (key) {
	case K_UP: sel = (sel + N - 1) % N; break;
	case K_DOWN: sel = (sel + 1) % N; break;
	case K_HOME: sel = 0; break;
	case K_END: sel = N - 1; break;
	case K_ESC: open = false; break;
	case K_ENTER: case K_KPENTER:
		msg[0] = 0;
		switch (sel) {
		case 0: case 1: case 2: enterBrowse(sel); break;
		case 3: a.type = A_EJECT_ROM; break;
		case 4: a.type = A_EJECT_CAS; break;
		case 5: a.type = A_EJECT_DSK; break;
		case 6: mapperIdx = (mapperIdx + 1) % 6; break;
		case 7: a.type = A_RESET; open = false; break;
		default: open = false; break;
		}
		break;
	default: break;
	}
}

void Menu::enterBrowse(int c)
{
	cat = c; page = P_BROWSE;
	copyStr(dir, sizeof dir, lastDir[c]);
	refresh();
}

void Menu::refresh()
{
	int n = src ? src->list(dir, cat, entries + 1, MAXE - 1) : 0;
	MenuEntry *e = entries + 1;
	for (int i = 1; i < n; i++) {			// insertion sort: dirs first, then case-insensitive name
		MenuEntry t = e[i]; int j = i - 1;
		while (j >= 0 && ((e[j].dir < t.dir) || (e[j].dir == t.dir && ci(e[j].name, t.name) > 0))) { e[j + 1] = e[j]; j--; }
		e[j + 1] = t;
	}
	if (dir[0]) { copyStr(entries[0].name, sizeof entries[0].name, ".."); entries[0].dir = true; first = 0; count = n + 1; }
	else { first = 1; count = n; }
	sel = 0; top = 0;
}

void Menu::goUp()
{
	char *s = m_strrchr(dir, '/');
	if (s) *s = 0; else dir[0] = 0;
	copyStr(lastDir[cat], sizeof lastDir[cat], dir);
	refresh();
}

void Menu::browseKey(uint8_t key, MenuAction &a)
{
	switch (key) {
	case K_UP: if (count) sel = (sel + count - 1) % count; break;
	case K_DOWN: if (count) sel = (sel + 1) % count; break;
	case K_PGUP: sel -= ROWS; if (sel < 0) sel = 0; break;
	case K_PGDN: sel += ROWS; if (sel >= count) sel = count ? count - 1 : 0; break;
	case K_HOME: sel = 0; break;
	case K_END: sel = count ? count - 1 : 0; break;
	case K_ESC: case K_BS:
		if (dir[0]) goUp(); else { page = P_MAIN; sel = cat; top = 0; }
		break;
	case K_ENTER: case K_KPENTER: {
		if (!count) break;
		const MenuEntry &e = entries[first + sel];
		if (e.dir) {
			if (!m_strcmp(e.name, "..")) { goUp(); break; }
			size_t dl = m_strlen(dir), nl = m_strlen(e.name);
			if (dl + nl + 2 >= sizeof dir) break;
			if (dl) { dir[dl] = '/'; m_strcpy(dir + dl + 1, e.name); } else m_strcpy(dir, e.name);
			copyStr(lastDir[cat], sizeof lastDir[cat], dir);
			refresh();
		} else {
			size_t dl = m_strlen(dir), nl = m_strlen(e.name);
			if (dl + nl + 2 >= sizeof a.path) break;
			if (dl) { m_strcpy(a.path, dir); a.path[dl] = '/'; m_strcpy(a.path + dl + 1, e.name); } else m_strcpy(a.path, e.name);
			a.type = cat == CAT_ROM ? A_LOAD_ROM : cat == CAT_CAS ? A_LOAD_CAS : A_LOAD_DSK;
			open = false;
		}
		break; }
	default: break;
	}
	fixScroll();
}

void Menu::rect(int x0, int y0, int x1, int y1, uint32_t c)
{
	for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) fb[y * 256 + x] = c;
}

void Menu::text(int x, int y, const char *s, uint32_t fg, uint32_t bg, bool opaque, const uint8_t *font)
{
	for (int i = 0; s[i] && i < COLS; i++, x += 8) {
		uint8_t ch = (uint8_t)s[i];
		for (int r = 0; r < 8; r++) {
			uint8_t bits = font ? font[ch * 8 + r] : (ch > 32 && r > 0 && r < 7 ? 0x7E : 0);
			for (int c = 0; c < 8; c++) {
				if (x + c >= 248) continue;
				if (bits & (0x80 >> c)) fb[(y + r) * 256 + x + c] = fg;
				else if (opaque) fb[(y + r) * 256 + x + c] = bg;
			}
		}
	}
}

const uint32_t *Menu::render(const uint32_t *bg, const uint8_t *font)
{
	const uint32_t NAVY = 0xFF101060, WHITE = 0xFFFFFFFF, YEL = 0xFFFFFF55, LINE = 0xFF8080FF;
	for (int i = 0; i < 256 * 192; i++) fb[i] = 0xFF000000 | ((bg[i] >> 1) & 0x7F7F7F);
	rect(8, 8, 247, 187, LINE);
	rect(9, 9, 246, 186, NAVY);
	rect(9, 21, 246, 21, LINE);
	rect(9, 172, 246, 172, LINE);

	char buf[128];
	if (page == P_MAIN) {
		text(16, 12, "MSX MENU", YEL, NAVY, false, font);
		for (int i = 0; i < 9; i++) {
			const char *s = mainItems[i];
			if (i == 6) { m_strcpy(buf, "ROM mapper: "); m_strcat(buf, Cart::name(mapper())); s = buf; }
			bool on = (i == sel);
			if (on) rect(12, 25 + i * 10, 243, 25 + i * 10 + 9, WHITE);
			text(16, 26 + i * 10, s, on ? NAVY : WHITE, on ? WHITE : NAVY, false, font);
		}
		for (int c = 0; c < 3; c++) {
			m_strcpy(buf, catNames[c]); m_strcat(buf, ": "); m_strncat(buf, loaded[c], 20);
			text(16, 26 + (10 + c) * 10, buf, 0xFFAAAAFF, NAVY, false, font);
		}
		if (msg[0]) text(16, 26 + 14 * 10, msg, YEL, NAVY, false, font);
		text(16, 176, "UP/DOWN ENTER ESC F12", 0xFFAAAAFF, NAVY, false, font);
	} else {
		m_strcpy(buf, catNames[cat]); m_strcat(buf, ": /");
		size_t dl = m_strlen(dir), room = COLS - m_strlen(buf);
		m_strcat(buf, dl > room ? dir + (dl - room) : dir);
		text(16, 12, buf, YEL, NAVY, false, font);
		if (!count) text(16, 26, "(no matching files)", WHITE, NAVY, false, font);
		for (int r = 0; r < ROWS && top + r < count; r++) {
			const MenuEntry &e = entries[first + top + r];
			bool on = (top + r == sel);
			if (e.dir) { buf[0] = '['; m_strncpy(buf + 1, e.name, COLS - 2); buf[COLS - 1] = 0; m_strcat(buf, "]"); }
			else copyStr(buf, sizeof buf, e.name);
			if (on) rect(12, 25 + r * 10, 243, 25 + r * 10 + 9, WHITE);
			text(16, 26 + r * 10, buf, on ? NAVY : (e.dir ? 0xFF88FFFF : WHITE), on ? WHITE : NAVY, false, font);
		}
		text(16, 176, "ENTER pick  BS/ESC back", 0xFFAAAAFF, NAVY, false, font);
	}
	return fb;
}
