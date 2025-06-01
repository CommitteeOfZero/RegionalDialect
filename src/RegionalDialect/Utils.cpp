#include <stdlib.h>
#include <string.h>

#include "util/sys/rw_pages.hpp"
#include "RegionalDialect/Utils.h"
#include <log/logger_mgr.hpp>


extern uintptr_t codeCaves;

namespace rd {
namespace utils {

uint32_t get_u32(uintptr_t address) {
    return *(uint32_t *)address;
}

uintptr_t get_ptr(uintptr_t address) {
    return *(uintptr_t*)address;
}

#define __flush_cache(c, n) __builtin___clear_cache(reinterpret_cast<char*>(c), reinterpret_cast<char*>(c) + n)

void overwrite_u32(uintptr_t address, uint32_t value) {
    exl::util::RwPages control(address, sizeof(uint32_t));
    uint32_t *rw = (uint32_t*)control.GetRw();
    *rw = value;
    control.Flush();
}

void memset(uintptr_t address, int8_t value, size_t size) {
    exl::util::RwPages control(address, size);
    uchar *rw = (uchar *)control.GetRw();
    ::memset(rw, value, size);
    control.Flush();

}

void overwrite_ptr(uintptr_t address, void *value) {
    exl::util::RwPages control(address, sizeof(void *));

    void **rw = (void **)control.GetRw();
    *rw = value;
    control.Flush();
}

void overwrite_b(uintptr_t address, intptr_t offset) {
    overwrite_u32(address, 0x14000000 | ((uint32_t)offset >> 2));
}

void overwrite_b_abs(uintptr_t address, uintptr_t target) {
    intptr_t offset = target - address;
    overwrite_b(address, offset);
}

void overwrite_trampoline(uintptr_t address, uintptr_t target,
                          uint8_t reg) {    
    if ((address & 3) || (target & 3) || reg > 31) std::abort();
    overwrite_b_abs(address, codeCaves);
    overwrite_u32(codeCaves + 0, 0x58000000 | (0x8 << 3) | reg);    // ldr Xn, 0x8
    overwrite_u32(codeCaves + 4, 0xD61F0000 | (reg << 5));          // br Xn
    overwrite_ptr(codeCaves + 8, (void*)target);                    // dq TARGET
    codeCaves += 0x10;
}

uintptr_t retrievePointer(uint64_t adrp_addr, uint64_t ldr_offset) {
    uint32_t pageAddrOffsetInst = get_u32(adrp_addr);
    Logging.Log("pageAddrOffsetInst: 0x%x\n", pageAddrOffsetInst);
    // Extracting the offset from the current page to the pointer's page from the instruction
    // immhi:immlo * 4096
    uint64_t immhi = (pageAddrOffsetInst >> 5) & 0x1FFF;
    uint64_t immlo = (pageAddrOffsetInst >> 29) & 0b11;
    uint64_t offsetFromPage = ((immhi << 2) | immlo) << 12;
    // Adding offset to current page address to get target page address
    uintptr_t pageAddr = (adrp_addr & 0xFFFFFFFFFFFFF000) + offsetFromPage;
    // Extracting the offset from the target page address to the pointer's address from the next instruction
    uint32_t offsetFromPageStartInst = get_u32(adrp_addr + ldr_offset);
    Logging.Log("offsetFromPageStartInst: 0x%x\n", offsetFromPageStartInst);
    uint32_t offsetFromPageStart = ((offsetFromPageStartInst >> 10) & 0xFFF) << (((offsetFromPageStartInst) >> 30) & 0b11);
    
    return pageAddr + offsetFromPageStart;
}

}  // namespace utils
}  // namespace rd