#include "kernel.h"
#include <circle/startup.h>

int main(void)
{
	CKernel Kernel;
	if (!Kernel.Initialize()) halt();

	TShutdownMode mode = Kernel.Run();
	switch (mode) {
	case ShutdownReboot: reboot(); break;
	case ShutdownHalt:
	default: halt(); break;
	}
	return EXIT_HALT;
}
