#include <Arduino.h>

#ifndef clearBit
#define clearBit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef setBit
#define setBit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif