// F12 on-screen menu: file browser for ROM / CAS / DSK + eject, mapper choice, reset.
// Pure logic + drawing into a 256x192 ARGB buffer (testable on the PC).
#ifndef MENU_H
#define MENU_H
#include <stdint.h>
#include "cart.h"

enum MenuCategory { CAT_ROM = 0, CAT_CAS = 1, CAT_DSK = 2 };
enum MenuActionType { A_NONE, A_LOAD_ROM, A_LOAD_CAS, A_LOAD_DSK, A_EJECT_ROM, A_EJECT_CAS, A_EJECT_DSK, A_RESET };

struct MenuAction { MenuActionType type; char path[128]; };	// path relative to the SD root, '/' separated
struct MenuEntry { char name[48]; bool dir; };

class FileSource
{
public:
	virtual ~FileSource() {}
	// List directory 'dir' ("" = root): all sub-directories + files matching the category. Returns count.
	virtual int list(const char *dir, int category, MenuEntry *out, int max) = 0;
};

class Menu
{
public:
	Menu(FileSource *src);
	bool isOpen() const { return open; }
	bool swallowing() const { return open || swallow; }	// true while keys must not reach the MSX
	MenuAction update(const uint8_t keys[6]);		// call once per frame with the pressed HID keys
	const uint32_t *render(const uint32_t *bg, const uint8_t *font);	// font: 256*8 bytes (MSX charset) or null
	MapperType mapper() const { return (MapperType)mapperIdx; }
	void setLoaded(int cat, const char *name);
	void setMessage(const char *m);
	static bool matches(int category, const char *name);

	enum { ROWS = 15, COLS = 28, MAXE = 256 };
private:
	enum Page { P_MAIN, P_BROWSE };
	FileSource *src;
	bool open, swallow;
	Page page;
	int sel, top, cat, first, count, mapperIdx;
	char dir[96], lastDir[3][96], loaded[3][48], msg[48];
	MenuEntry entries[MAXE + 1];
	bool was[11]; int hold[11];
	uint32_t fb[256 * 192];

	void handle(uint8_t key, MenuAction &a);
	void mainKey(uint8_t key, MenuAction &a);
	void browseKey(uint8_t key, MenuAction &a);
	void enterBrowse(int c);
	void refresh();
	void goUp();
	void fixScroll();
	void text(int x, int y, const char *s, uint32_t fg, uint32_t bg, bool opaque, const uint8_t *font);
	void rect(int x0, int y0, int x1, int y1, uint32_t c);
	int items() const { return page == P_MAIN ? 9 : count; }
};

#endif
