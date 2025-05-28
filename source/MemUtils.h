#ifndef __MEMUTILS_H__
#define __MEMUTILS_H__

#include <cstdint>

uint32_t get_u32(uintptr_t address);

uintptr_t get_ptr(uintptr_t address);

void overwrite_u32(uintptr_t address, uint32_t value);

// TODO: Fix name when we add namespaces
void _memset(uintptr_t address, uchar value, size_t size);

void overwrite_ptr(uintptr_t address, void *value);

void overwrite_b(uintptr_t address, intptr_t offset);

void overwrite_b_abs(uintptr_t address, uintptr_t target);

void overwrite_trampoline(uintptr_t address, uintptr_t target, uint8_t reg);

uintptr_t retrievePointer(uint64_t adrp_addr, uint64_t ldr_offset = 0x4);

#endif  // !__MEMUTILS_H__