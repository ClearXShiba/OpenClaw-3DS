#ifndef OPENCLAW_3DS_COMPATTYPES_H
#define OPENCLAW_3DS_COMPATTYPES_H

// Force-included into every C++ file by 3ds/Makefile.
//
// arm-none-eabi (newlib) defines int32_t as 'long' and uint32_t as 'unsigned long'.
// OpenClaw and Box2D mix int / int32 / int32_t freely, which only works when all of them
// are the same type (true on PC). The Makefile passes -D__INT32_TYPE__=int (and the unsigned
// equivalent) so newlib's <stdint.h> picks 'int' instead. This check fails the build early,
// with a readable message, if a toolchain ignores that.

#ifdef __cplusplus
#include <stdint.h>
#include <type_traits>
static_assert(std::is_same<int32_t, int>::value && std::is_same<uint32_t, unsigned int>::value,
              "int32_t/uint32_t are not int/unsigned int - the -D__INT32_TYPE__=int override did not take effect");
#endif

#endif
