#ifndef CART_H
#define CART_H
#include <stdint.h>
#include <stddef.h>

enum MapperType { M_AUTO, M_PLAIN, M_KONAMI, M_KONAMI_SCC, M_ASCII8, M_ASCII16 };

class Cart
{
public:
	Cart() : rom(0), size(0), type(M_PLAIN), base(0x4000) {}
	void load(const uint8_t *data, size_t n, MapperType t);	// data must stay valid
	void eject() { rom = 0; size = 0; }
	bool present() const { return rom != 0; }
	uint8_t read(uint16_t a) const;
	void write(uint16_t a, uint8_t v);
	MapperType mapper() const { return type; }
	static MapperType detect(const uint8_t *d, size_t n);
	static const char *name(MapperType t);
private:
	const uint8_t *rom; size_t size; MapperType type;
	uint16_t base; unsigned nb8, nb16; uint8_t bank[4];
};

#endif
