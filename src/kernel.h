#ifndef KERNEL_H
#define KERNEL_H

#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/bcmframebuffer.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/types.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include "msx.h"
#include "menu.h"

enum TShutdownMode { ShutdownNone, ShutdownHalt, ShutdownReboot };

class CKernel
{
public:
	CKernel(void);
	~CKernel(void);
	boolean Initialize(void);
	TShutdownMode Run(void);

private:
	u8 *LoadFile(const char *name, size_t *size);
	void Fail(const char *msg, u32 color);
	void Blit(const u32 *src);
	void HandleAction(const MenuAction &a);
	void InitAudio(const char *mode);
	void PumpAudio(void);
	void DrawOverlays(u32 *img);
	void ReplaceBuf(u8 **slot, u8 *buf);
	void PollKeyboard(void);
	static void KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]);

private:
	CMemorySystem		m_Memory;
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CBcmFrameBuffer		m_FB;
	CUSBHCIDevice		m_USBHCI;
	CEMMCDevice		m_EMMC;
	FATFS			m_FileSystem;

	Msx			*m_pMsx;
	CUSBKeyboardDevice	*m_pKeyboard;
	Menu			*m_pMenu;
	FileSource		*m_pLister;
	u8			*m_pCartData, *m_pCasData, *m_pDskData;
	volatile u8		m_Raw[6];
	volatile u8		m_Mods;
	CSoundBaseDevice	*m_pSound;
	unsigned		m_nSoundRate, m_nSoundAcc;
	boolean			m_bF11;
	boolean			m_bTapeBar, m_bDebug;
	unsigned		m_nFrameNo, m_nLoopNo;
	boolean			m_bLed;
	u32			*m_pComposite;		// 256x192 picture + overlays, drawn in normal RAM
	unsigned		m_nSpeedMsg;		// frames left to show the tape speed indicator

	static CKernel *s_pThis;
};

#endif
