# Build with Circle (https://github.com/rsta2/circle) for Raspberry Pi 3, 64 bit.
# Set CIRCLEHOME to your Circle checkout (default: ../../circle) and configure Circle first:
#   cd circle && ./configure -r 3 -p aarch64-none-elf-
CIRCLEHOME ?= ../../circle

OBJS	= main.o kernel.o compat.o menu.o msx.o z80.o vdp.o psg.o cart.o tape.o keyboard.o

LIBS	= $(CIRCLEHOME)/lib/sound/libsound.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

# Flags needed when the cross compiler is a Linux distribution one (Ubuntu/Debian aarch64-linux-gnu-):
# those enable hardening features that rely on glibc/Linux and do not exist on bare metal.
MSXFLAGS = -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -fno-pie -mno-outline-atomics -mbranch-protection=none
CFLAGS   += $(MSXFLAGS)
CPPFLAGS += $(MSXFLAGS)

-include $(DEPS)
