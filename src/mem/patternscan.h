#ifndef PATTERN_SCAN_H
#define PATTERN_SCAN_H

#include "stdint.h"

/*
  This is a very powerful pattern scanner.
  It uses IDA style patterns with some extra features. Take for example this pattern:
  34 24 [E8 *? ? ? ?] E9 DE FD FF FF
  Here are the steps that are taken in the scanner:
  First of all, the pattern 34 24 E8 ? ? ? ? E9 DE FD FF FF is parsed and its address is found.
  Then, if the address is not null, the asterix in [E8 *? ? ? ?] is dereferenced relative to the brackets and this is our final address.
  In short, the scanner does not only find the pattern, it also allows to get the address of the exact element we want in memory. Here is the full list of actions we can take:
  1) Dereference native size pointer with *
  2) Read 8-bit size value with $
  3) Read 16-bit size value with %
  4) Read 32-bit size value with ^
  5) Read 64-bit size value with &
  6) Offset the resulting address to the wanted place with @
  7) Instruct the first dereference to be IP-relative with []
  8) After each dereference, an offset can be specified with +NUM or -NUM, a space is needed if the next non-offset character is a part of exact pattern match
  9) Manual relative dereference is done with :OFF,SIZE
*/

namespace PatternScan
{
	uintptr_t FindPattern(const char* pattern, uintptr_t start, uintptr_t end);
	uintptr_t FindPattern(const char* __restrict pattern, const char* __restrict module);
}

#endif
