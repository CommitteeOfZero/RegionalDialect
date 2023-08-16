#ifndef __SIGSCAN_H__
#define __SIGSCAN_H__

#include <cstdint>

uintptr_t FindPattern(const unsigned char* dataStart,
                      const unsigned char* dataEnd, const char* pszPattern,
                      uintptr_t baseAddress, size_t offset, int occurrence);

uintptr_t sigScan(const char* category, const char* sigName);


#endif  // !__SIGSCAN_H__
