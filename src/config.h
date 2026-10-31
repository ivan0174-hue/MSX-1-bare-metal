#ifndef CONFIG_H
#define CONFIG_H
// Framebuffer pixel format. Default 0xAARRGGBB. If red and blue look swapped on your Pi,
// define FB_SWAP_RB (the VDP palette is produced as ARGB).
// #define FB_SWAP_RB
// Diagnostic build: blink the green LED N times at start-up stage N (1..10). Comment out when it works.
// #define MSX_DIAG
// Hot-plug of the USB keyboard (calls UpdatePlugAndPlay every frame). Off by default: the keyboard must be
// plugged in before power-on. Enable only if your Circle version handles it (it hung on the first test).
// #define MSX_USB_HOTPLUG
#define MSX_FRAME_US	16688		// 262 lines * 228 cycles @ 3.579545 MHz
#define FB_SCALE	2		// 256x192 -> 512x384
#endif
