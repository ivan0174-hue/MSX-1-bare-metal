// Bare-metal glue for toolchains meant for Linux (e.g. Ubuntu's aarch64-linux-gnu-*).
// libgcc's outline-atomics init asks Linux for the CPU capabilities; there is no Linux here,
// so report "none" and let it use the load/store-exclusive fallback (fine on Cortex-A53).
extern "C" unsigned long __getauxval(unsigned long type)
{
	(void)type;
	return 0;
}
