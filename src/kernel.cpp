#include "kernel.h"
#include "config.h"
#include "keyboard.h"
#include <circle/string.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <string.h>
#include "mstr.h"

#define DRIVE "SD:"
static const char FromKernel[] = "msx";

// Diagnostic build: the green ACT LED blinks N times at each start-up stage (see config.h, MSX_DIAG).
// If the board resets or hangs, the last number seen tells where it stopped.
#ifdef MSX_DIAG
#define STAGE(n) do { m_ActLED.Blink(n, 150, 150); CTimer::SimpleMsDelay(1000); } while (0)
#else
#define STAGE(n) do { } while (0)
#endif

CKernel *CKernel::s_pThis = 0;

// 3x5 hex digit font for the on-screen diagnostic overlay
static const u8 kHex[16][5] = {
	{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},
	{7,5,7,5,7},{7,5,7,1,7},{7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,7,4,7},{7,4,7,4,4}
};

static void DiagRect(u8 *fb, unsigned pitch, int x, int y, int w, int h, u32 color)
{
	for (int yy = 0; yy < h; yy++) {
		u32 *row = (u32 *)(fb + (y + yy) * pitch);
		for (int xx = 0; xx < w; xx++) row[x + xx] = color;
	}
}

// white hex digits on black, each font pixel = 4x4 screen pixels
static void DiagHex(u8 *fb, unsigned pitch, u32 value, int digits, int x, int y, int S = 4)
{
	DiagRect(fb, pitch, x - S, y - S, digits * 4 * S + S, 7 * S, 0xFF000000);
	for (int d = 0; d < digits; d++) {
		unsigned nib = (value >> ((digits - 1 - d) * 4)) & 15;
		for (int r = 0; r < 5; r++)
			for (int c = 0; c < 3; c++)
				if (kHex[nib][r] & (4 >> c))
					DiagRect(fb, pitch, x + d * 4 * S + c * S, y + r * S, S, S, 0xFFFFFFFF);
	}
}

static const u8 kGlyphX[5] = { 5, 5, 2, 5, 5 };

static void DrawGlyph(u8 *fb, unsigned pitch, const u8 *g, int x, int y, int S, u32 fg)
{
	for (int r = 0; r < 5; r++)
		for (int c = 0; c < 3; c++)
			if (g[r] & (4 >> c)) DiagRect(fb, pitch, x + c * S, y + r * S, S, S, fg);
}

// decimal number (up to 3 digits, no leading zeros) on a black box; returns the box width
static int DrawDec(u8 *fb, unsigned pitch, unsigned v, int x, int y, int S, u32 fg)
{
	char d[3]; int n = 0;
	if (v > 999) v = 999;
	if (v >= 100) d[n++] = v / 100;
	if (v >= 10) d[n++] = (v / 10) % 10;
	d[n++] = v % 10;
	int w = n * 4 * S + S;
	DiagRect(fb, pitch, x - S, y - S, w + S, 7 * S, 0xFF000000);
	for (int i = 0; i < n; i++) DrawGlyph(fb, pitch, kHex[(int)d[i]], x + i * 4 * S, y, S, fg);
	return w;
}

#ifdef MSX_DIAG
// 8 vertical bars: red, green, blue, white, yellow, cyan, magenta, grey
static void DiagBars(u8 *fb, unsigned pitch, int width, int height)
{
	static const u32 col[8] = { 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF,
				    0xFFFFFF00, 0xFF00FFFF, 0xFFFF00FF, 0xFF808080 };
	for (int i = 0; i < 8; i++) DiagRect(fb, pitch, i * (width / 8), 0, width / 8, height, col[i]);
}

static u8 *g_dFb = 0;
static unsigned g_dPitch = 0;

// called by the emulator every 16 scanlines: shows the scanline and the Z80 PC where it is right now
static void DiagHook(int line, uint16_t pc)
{
	if (!g_dFb) return;
	DiagHex(g_dFb, g_dPitch, (u32)line, 3, 8, 200);
	DiagHex(g_dFb, g_dPitch, pc, 4, 8, 232);
}
#define MARK(n) DiagRect((u8 *)(uintptr)m_FB.GetBuffer(), m_FB.GetPitch(), 8 + (n) * 28, 340, 20, 20, 0xFFFFFFFF)
#else
#define MARK(n) do { } while (0)
#endif


CKernel::CKernel(void)
:	m_Timer(&m_Interrupt),
	m_Logger(m_Options.GetLogLevel(), &m_Timer),
	m_FB(FB_SCALE * 256, FB_SCALE * 192, 32),
	m_USBHCI(&m_Interrupt, &m_Timer),
	m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
	m_pMsx(0),
	m_pKeyboard(0),
	m_pMenu(0),
	m_pLister(0),
	m_pCartData(0), m_pCasData(0), m_pDskData(0),
	m_Mods(0),
	m_pSound(0), m_nSoundRate(0), m_nSoundAcc(0), m_bF11(FALSE), m_bTapeBar(TRUE), m_bDebug(FALSE), m_nFrameNo(0), m_nLoopNo(0), m_bLed(FALSE), m_pComposite(0), m_nSpeedMsg(0)
{
	s_pThis = this;
	memset((void *)m_Raw, 0, sizeof m_Raw);
}

CKernel::~CKernel(void) {}

boolean CKernel::Initialize(void)
{
	boolean ok = TRUE;
	STAGE(1); if (ok) ok = m_Serial.Initialize(115200);
	STAGE(2); if (ok) ok = m_Logger.Initialize(&m_Serial);
	STAGE(3); if (ok) ok = m_Interrupt.Initialize();
	STAGE(4); if (ok) ok = m_Timer.Initialize();
	STAGE(5); if (ok) ok = m_FB.Initialize();
	STAGE(6); if (ok) ok = m_USBHCI.Initialize();
	STAGE(7); if (ok) ok = m_EMMC.Initialize();
	return ok;
}

// Error screen: solid color + message on the serial console (115200 8N1, GPIO14/15)
void CKernel::Fail(const char *msg, u32 color)
{
	m_Logger.Write(FromKernel, LogError, "%s", msg);
	u8 *fb = (u8 *)(uintptr)m_FB.GetBuffer();
	unsigned pitch = m_FB.GetPitch();
	for (unsigned y = 0; y < FB_SCALE * 192; y++) {
		u32 *row = (u32 *)(fb + y * pitch);
		for (unsigned x = 0; x < FB_SCALE * 256; x++) row[x] = color;
	}
	for (;;) m_Timer.MsDelay(1000);
}

u8 *CKernel::LoadFile(const char *name, size_t *size)
{
	CString path; path.Format("%s/%s", DRIVE, name);
	FIL f;
	if (f_open(&f, (const char *)path, FA_READ) != FR_OK) return 0;
	size_t n = f_size(&f);
	u8 *buf = new u8[n ? n : 1];
	UINT got = 0;
	if (f_read(&f, buf, n, &got) != FR_OK || got != n) { f_close(&f); delete[] buf; return 0; }
	f_close(&f);
	*size = n;
	return buf;
}

// Lists an SD directory for the menu (sub-directories + files of the requested category)
class CFatLister : public FileSource
{
public:
	int list(const char *dir, int category, MenuEntry *out, int max)
	{
		CString path; path.Format("%s/%s", DRIVE, dir);
		DIR d; FILINFO fno; int n = 0;
		if (f_opendir(&d, (const char *)path) != FR_OK) return 0;
		while (n < max && f_readdir(&d, &fno) == FR_OK && fno.fname[0]) {
			if (fno.fattrib & (AM_HID | AM_SYS)) continue;
			if (fno.fname[0] == '.') continue;
			if (m_strlen(fno.fname) >= sizeof out[0].name) continue;
			bool isdir = (fno.fattrib & AM_DIR) != 0;
			if (!isdir && !Menu::matches(category, fno.fname)) continue;
			m_strcpy(out[n].name, fno.fname); out[n].dir = isdir; n++;
		}
		f_closedir(&d);
		return n;
	}
};

// msx.cfg: lines "key=value" with keys bios, cart, cas, mapper (auto|plain|konami|scc|ascii8|ascii16)
struct Config { char bios[64], cart[64], cas[64], mapper[16], audio[8], tapebar[8], debug[8]; };

static void ParseConfig(const u8 *d, size_t n, Config *c)
{
	size_t i = 0;
	while (i < n) {
		char line[96]; size_t l = 0;
		while (i < n && d[i] != '\n' && d[i] != '\r') { if (l < sizeof line - 1) line[l++] = d[i]; i++; }
		while (i < n && (d[i] == '\n' || d[i] == '\r')) i++;
		line[l] = 0;
		char *eq = m_strchr(line, '=');
		if (!eq || line[0] == '#') continue;
		*eq = 0; char *val = eq + 1;
		char *dst = 0; size_t max = 0;
		if (!m_strcmp(line, "bios")) { dst = c->bios; max = sizeof c->bios; }
		else if (!m_strcmp(line, "cart")) { dst = c->cart; max = sizeof c->cart; }
		else if (!m_strcmp(line, "cas")) { dst = c->cas; max = sizeof c->cas; }
		else if (!m_strcmp(line, "mapper")) { dst = c->mapper; max = sizeof c->mapper; }
		else if (!m_strcmp(line, "audio")) { dst = c->audio; max = sizeof c->audio; }
		else if (!m_strcmp(line, "tapebar")) { dst = c->tapebar; max = sizeof c->tapebar; }
		else if (!m_strcmp(line, "debug")) { dst = c->debug; max = sizeof c->debug; }
		if (dst) { m_strncpy(dst, val, max - 1); dst[max - 1] = 0; }
	}
}

void CKernel::KeyStatusHandlerRaw(unsigned char mods, const unsigned char keys[6])
{
	s_pThis->m_Mods = mods;
	for (int i = 0; i < 6; i++) s_pThis->m_Raw[i] = keys[i];
}

void CKernel::PollKeyboard(void)
{
	MARK(6);
#ifdef MSX_USB_HOTPLUG
	m_USBHCI.UpdatePlugAndPlay();	// only needed for keyboards plugged in while running
#endif
	MARK(7);
	CUSBKeyboardDevice *kb = (CUSBKeyboardDevice *)m_DeviceNameService.GetDevice("ukbd1", FALSE);
	MARK(8);
	if (kb != m_pKeyboard) {
		m_pKeyboard = kb;
		if (kb) { kb->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw); MARK(9); }
		else { memset((void *)m_Raw, 0, sizeof m_Raw); m_Mods = 0; }
	}
}

void CKernel::Blit(const u32 *src)
{
	u8 *fb = (u8 *)(uintptr)m_FB.GetBuffer();
	unsigned pitch = m_FB.GetPitch();
	for (int y = 0; y < 192; y++) {
		for (int sy = 0; sy < FB_SCALE; sy++) {
			u32 *dst = (u32 *)(fb + (y * FB_SCALE + sy) * pitch);
			for (int x = 0; x < 256; x++) {
				u32 c = src[y * 256 + x];
#ifdef FB_SWAP_RB
				c = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
#endif
				for (int sx = 0; sx < FB_SCALE; sx++) dst[x * FB_SCALE + sx] = c;
			}
		}
	}
}

void CKernel::ReplaceBuf(u8 **slot, u8 *buf)
{
	u8 *old = *slot;
	*slot = buf;
	delete[] old;		// only after the emulator switched to the new buffer
}

static const char *BaseName(const char *p)
{
	const char *s = m_strrchr(p, '/');
	return s ? s + 1 : p;
}

void CKernel::HandleAction(const MenuAction &a)
{
	size_t n = 0;
	switch (a.type) {
	case A_LOAD_ROM: {
		u8 *d = LoadFile(a.path, &n);
		if (!d) { m_pMenu->setMessage("Cannot read file"); break; }
		m_pMsx->setCart(d, n, m_pMenu->mapper());
		ReplaceBuf(&m_pCartData, d);
		m_pMsx->hardReset();
		m_pMenu->setLoaded(CAT_ROM, BaseName(a.path));
		if (m_pMsx->systemRom()) m_pMenu->setMessage("System ROM: runs without BIOS");
		m_Logger.Write(FromKernel, LogNotice, "ROM %s, %s", a.path, m_pMsx->systemRom() ? "system ROM (no BIOS)" : Cart::name(m_pMsx->cart.mapper()));
		break; }
	case A_LOAD_CAS: {
		u8 *d = LoadFile(a.path, &n);
		if (!d) { m_pMenu->setMessage("Cannot read file"); break; }
		m_pMsx->setCas(d, n);
		ReplaceBuf(&m_pCasData, d);
		m_pMenu->setLoaded(CAT_CAS, BaseName(a.path));
		break; }
	case A_LOAD_DSK: {
		u8 *d = LoadFile(a.path, &n);
		if (!d) { m_pMenu->setMessage("Cannot read file"); break; }
		m_pMsx->setDisk(d, n);
		ReplaceBuf(&m_pDskData, d);
		m_pMenu->setLoaded(CAT_DSK, BaseName(a.path));
		m_Logger.Write(FromKernel, LogNotice, "Disk image %s (%u bytes) - controller not emulated yet", a.path, (unsigned)n);
		break; }
	case A_EJECT_ROM:
		m_pMsx->ejectCart(); ReplaceBuf(&m_pCartData, 0); m_pMsx->hardReset();
		m_pMenu->setLoaded(CAT_ROM, 0); m_pMenu->setMessage("Cartridge removed, MSX reset");
		break;
	case A_EJECT_CAS:
		m_pMsx->tape.eject(); ReplaceBuf(&m_pCasData, 0); m_pMenu->setLoaded(CAT_CAS, 0);
		break;
	case A_EJECT_DSK:
		m_pMsx->setDisk(0, 0); ReplaceBuf(&m_pDskData, 0); m_pMenu->setLoaded(CAT_DSK, 0);
		break;
	case A_RESET: m_pMsx->hardReset(); break;
	default: break;
	}
}

// Sound output through Circle's queued sound API: audio=hdmi (monitor/TV speakers), jack (3.5 mm), off
void CKernel::InitAudio(const char *mode)
{
	if (!m_strcmp(mode, "off")) return;

	if (!m_strcmp(mode, "jack")) { m_nSoundRate = 44100; m_pSound = new CPWMSoundBaseDevice(&m_Interrupt, m_nSoundRate); }
	else			   { m_nSoundRate = 48000; m_pSound = new CHDMISoundBaseDevice(&m_Interrupt, m_nSoundRate); }

	if (!m_pSound->AllocateQueue(200)) {
		m_Logger.Write(FromKernel, LogError, "Cannot allocate sound queue: audio off");
		delete m_pSound; m_pSound = 0; return;
	}
	m_pSound->SetWriteFormat(SoundFormatSigned16, 1);			// mono, 16 bit

	// pre-fill with ~4 frames of silence so the first frames do not run dry
	static s16 silence[1024];
	unsigned frames = 4 * (m_nSoundRate / 60);
	while (frames) {
		unsigned n = frames < 1024 ? frames : 1024;
		m_pSound->Write(silence, n * sizeof(s16));
		frames -= n;
	}
	if (!m_pSound->Start()) {
		m_Logger.Write(FromKernel, LogError, "Cannot start sound device: audio off");
		delete m_pSound; m_pSound = 0; return;
	}
	m_pMsx->psg.setLogging(true);						// sample-accurate PSG register writes
	m_Logger.Write(FromKernel, LogNotice, "Audio %s, %u Hz", mode, m_nSoundRate);
}

// Called once per emulated frame (16.688 ms): hand the PSG samples of that frame to the sound queue.
void CKernel::PumpAudio(void)
{
	if (!m_pSound) return;

	m_nSoundAcc += m_nSoundRate * MSX_FRAME_US;		// samples per frame, keeping the remainder
	unsigned n = m_nSoundAcc / 1000000;
	m_nSoundAcc %= 1000000;
	if (n > 1024) n = 1024;

	static s16 buf[1024];
	m_pMsx->renderAudio(buf, (int)n, (int)m_nSoundRate);

	unsigned queued = m_pSound->GetQueueFramesAvail();
	if (queued < 2 * n) {					// after a pause (menu, file load): refill the margin
		static s16 silence[1024];
		unsigned pad = 2 * n - queued;
		while (pad) {
			unsigned c = pad < 1024 ? pad : 1024;
			m_pSound->Write(silence, c * sizeof(s16));
			pad -= c;
		}
	} else if (queued > 8 * n) {
		return;						// too much queued: drop this frame to keep the latency low
	}
	m_pSound->Write(buf, n * sizeof(s16));
}

// On-screen indicators, drawn into a 256x192 copy of the picture (the Blit then scales it to the screen):
// tape progress bar, tape speed message, debug numbers
void CKernel::DrawOverlays(u32 *img)
{
	u8 *fb = (u8 *)img;
	const unsigned pitch = 256 * sizeof(u32);
	const int W = 256, H = 192;
	Tape &tape = m_pMsx->tape;

	if (m_bTapeBar && tape.active()) {			// tape running: progress bar at the bottom
		const int x0 = 40, x1 = W - 8, y = H - 12, bh = 6;
		u32 color = tape.finished() ? 0xFFE03030 :		// red = end of tape reached
			    tape.playingFast() ? 0xFFFFA000 : 0xFF20D060;	// orange = double speed, green = normal
		DiagRect(fb, pitch, x0 - 1, y - 1, x1 - x0 + 2, bh + 2, 0xFFFFFFFF);
		DiagRect(fb, pitch, x0, y, x1 - x0, bh, 0xFF000000);
		DiagRect(fb, pitch, x0, y, (int)((x1 - x0) * (u64)tape.percent() / 100), bh, color);
		DrawDec(fb, pitch, tape.percent(), 6, H - 17, 2, 0xFFFFFFFF);
		if (tape.playingFast()) DrawGlyph(fb, pitch, kGlyphX, x1 - 8, y - 12, 2, 0xFFFFA000);
	}
	if (m_bDebug) {						// debug=on in msx.cfg: frame counter, Z80 PC, tape state
		DiagHex(fb, pitch, m_nFrameNo, 4, 6, 8, 2);
		DiagHex(fb, pitch, m_pMsx->pc.w, 4, 6, 28, 2);
		DiagHex(fb, pitch, tape.percent() | (tape.active() ? 0x100 : 0), 3, 6, 48, 2);
	}
	if (m_nSpeedMsg) {					// brief confirmation after F11: "1X" or "2X" (top right)
		m_nSpeedMsg--;
		DiagRect(fb, pitch, W - 40, 4, 34, 24, 0xFF000000);
		DrawGlyph(fb, pitch, kHex[tape.fast() ? 2 : 1], W - 36, 8, 3, tape.fast() ? 0xFFFFA000 : 0xFF20D060);
		DrawGlyph(fb, pitch, kGlyphX, W - 36 + 12, 8, 3, 0xFFFFFFFF);
	}
}

TShutdownMode CKernel::Run(void)
{
	m_Logger.Write(FromKernel, LogNotice, "MSX1 bare metal emulator - phase 1");

	if (f_mount(&m_FileSystem, DRIVE, 1) != FR_OK)
		Fail("Cannot mount SD card (FAT32 expected)", 0xFFFF0000);		// red
	STAGE(8);

	Config cfg; memset(&cfg, 0, sizeof cfg);
	m_strcpy(cfg.bios, "msx1.rom"); m_strcpy(cfg.mapper, "auto"); m_strcpy(cfg.audio, "hdmi"); m_strcpy(cfg.tapebar, "on"); m_strcpy(cfg.debug, "off");
	size_t n = 0;
	if (u8 *c = LoadFile("msx.cfg", &n)) { ParseConfig(c, n, &cfg); delete[] c; }

	m_pMsx = new Msx;

	u8 *bios = LoadFile(cfg.bios, &n);
	if (!bios || !m_pMsx->setBios(bios, n))
		Fail("BIOS ROM not found or too small (need 32K MSX1 BIOS)", 0xFFFFA500);	// orange
	delete[] bios;
	STAGE(9);

	if (cfg.cart[0]) {
		u8 *rom = LoadFile(cfg.cart, &n);			// kept allocated: the cartridge reads from it
		if (!rom) Fail("Cartridge ROM not found", 0xFFFF00FF);				// magenta
		MapperType mt = M_AUTO;
		if (!m_strcmp(cfg.mapper, "plain")) mt = M_PLAIN;
		else if (!m_strcmp(cfg.mapper, "konami")) mt = M_KONAMI;
		else if (!m_strcmp(cfg.mapper, "scc")) mt = M_KONAMI_SCC;
		else if (!m_strcmp(cfg.mapper, "ascii8")) mt = M_ASCII8;
		else if (!m_strcmp(cfg.mapper, "ascii16")) mt = M_ASCII16;
		m_pMsx->setCart(rom, n, mt);
		m_pCartData = rom;
		m_Logger.Write(FromKernel, LogNotice, "Cartridge %s (%u bytes), mapper %s",
			       cfg.cart, (unsigned)n, Cart::name(m_pMsx->cart.mapper()));
	}
	if (cfg.cas[0]) {
		u8 *cas = LoadFile(cfg.cas, &n);
		if (!cas) Fail("CAS file not found", 0xFF00FFFF);					// cyan
		m_pMsx->setCas(cas, n);
		m_pCasData = cas;
		m_Logger.Write(FromKernel, LogNotice, "Tape %s (%u bytes)", cfg.cas, (unsigned)n);
	}

	m_pLister = new CFatLister;
	m_pMenu = new Menu(m_pLister);
	m_bTapeBar = m_strcmp(cfg.tapebar, "off") != 0;
	m_bDebug = !m_strcmp(cfg.debug, "on");
	m_pComposite = new u32[256 * 192];
	InitAudio(cfg.audio);
	if (cfg.cart[0]) m_pMenu->setLoaded(CAT_ROM, cfg.cart);
	if (cfg.cas[0]) m_pMenu->setLoaded(CAT_CAS, cfg.cas);
	if (!m_pMsx->charset()) m_Logger.Write(FromKernel, LogWarning, "BIOS has no usable charset: menu text unreadable");

#ifdef MSX_DIAG
	DiagBars((u8 *)(uintptr)m_FB.GetBuffer(), m_FB.GetPitch(), FB_SCALE * 256, FB_SCALE * 192);
	m_Timer.MsDelay(3000);		// color test: expect red, green, blue, white, yellow, cyan, magenta, grey
	g_dFb = (u8 *)(uintptr)m_FB.GetBuffer(); g_dPitch = m_FB.GetPitch();
	m_pMsx->dbgHook = DiagHook;
#endif
	STAGE(10);
	unsigned frame = 0; (void)frame;
	unsigned next = m_Timer.GetClockTicks();
	for (;;) {
		MARK(0);
#ifdef MSX_DIAG
		if (frame >= 180)		// diagnostic: let the emulator run ~3 s before touching USB
#endif
		PollKeyboard();
		MARK(1);
		uint8_t keys[6], mods = m_Mods;
		for (int i = 0; i < 6; i++) keys[i] = m_Raw[i];

		bool f11 = false;					// F11 toggles tape speed 1x / 2x (applies from the next block)
		for (int i = 0; i < 6; i++) if (keys[i] == 0x44) f11 = true;
		if (f11 && !m_bF11) { m_pMsx->tape.setFast(!m_pMsx->tape.fast()); m_nSpeedMsg = 90; }
		m_bF11 = f11;

		MenuAction act = m_pMenu->update(keys);
		if (act.type != A_NONE) HandleAction(act);
		MARK(2);

		if (m_pMenu->isOpen()) {			// emulation paused while the menu is open
			Blit(m_pMenu->render(m_pMsx->vdp.frame(), m_pMsx->charset()));
		} else {
			uint8_t mat[11];
			if (m_pMenu->swallowing()) memset(mat, 0, sizeof mat);	// ignore keys still held from the menu
			else hidToMatrix(mods, keys, mat);
			m_pMsx->setKeys(mat);
			MARK(3);
			m_pMsx->runFrame();
			MARK(4);
			PumpAudio();
			memcpy(m_pComposite, m_pMsx->vdp.frame(), 256 * 192 * sizeof(u32));
			m_nFrameNo++;
			DrawOverlays(m_pComposite);
			Blit(m_pComposite);
		}

#ifdef MSX_DIAG
		{
			u8 *fbp = (u8 *)(uintptr)m_FB.GetBuffer(); unsigned pt = m_FB.GetPitch();
			DiagHex(fbp, pt, frame++, 4, 8, 8);				// frame counter
			DiagHex(fbp, pt, m_pMsx->pc.w, 4, 8, 40);			// Z80 program counter
			DiagHex(fbp, pt, ((u32)m_pMsx->ppiA << 16) | (m_pMsx->vdp.r[0] << 8) | m_pMsx->vdp.r[1], 6, 8, 72);	// slots, VDP R0, VDP R1
		}
#endif
		if (m_bDebug && (++m_nLoopNo % 30) == 0) {	// heartbeat: the green LED blinks while the main loop is alive
			m_bLed = !m_bLed;
			if (m_bLed) m_ActLED.On(); else m_ActLED.Off();
		}
		next += MSX_FRAME_US;
		if ((int)(m_Timer.GetClockTicks() - next) > 100000) next = m_Timer.GetClockTicks();	// resync if far behind
		while ((int)(m_Timer.GetClockTicks() - next) < 0) { }
	}
	return ShutdownHalt;
}
