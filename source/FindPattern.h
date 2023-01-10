#ifndef _FINDPATTERN_H
#define _FINDPATTERN_H

#include <vector>

/**
* @brief Scans a given chunk of data for a given pattern.
*
* @param data          The data to scan within for the given pattern.
* @param pszPattern    The pattern to scan for.
* @param baseAddress   The base address to add to the final result.
* @param offset        The offset to add to final address.
* @param occurrence    Zero-based occurrence of the pattern to return.
*
* @return Address of the pattern found, -1 otherwise.
*/
uintptr_t FindPattern(const unsigned char* dataStart,
                      const unsigned char* dataEnd, const char* pszPattern,
                      uintptr_t baseAddress, size_t offset, int occurrence);

#endif //_FINDPATTERN_H