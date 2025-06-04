#include "log/logger_mgr.hpp"

#include "RegionalDialect/Utils.h"

extern uintptr_t codeCaves;

namespace rd {
namespace utils {

void Trampoline(uintptr_t address, uintptr_t target, reg::Register reg) {    
    if ((address & 3) || (target & 3) || reg.Index() > 31) ::abort();
    Overwrite(address,          inst::BranchLink(codeCaves - address).Value());
    Overwrite(codeCaves + 0,    inst::LdrLiteral(reg, 0x8).Value());
    Overwrite(codeCaves + 4,    inst::BranchRegister(reg).Value());
    Overwrite(codeCaves + 8,    target);
    codeCaves += 0x10;
}

uintptr_t AssemblePointer(uintptr_t adrp_addr, ptrdiff_t ldr_offset) {
    uint32_t pageAddrOffsetInst = *reinterpret_cast<uint32_t*>(adrp_addr);
    // Extracting the offset from the current page to the pointer's page from the instruction
    // immhi:immlo * 4096
    uint64_t immhi = (pageAddrOffsetInst >> 5) & 0x1FFF;
    uint64_t immlo = (pageAddrOffsetInst >> 29) & 0b11;
    uint64_t offsetFromPage = ((immhi << 2) | immlo) << 12;
    // Adding offset to current page address to get target page address
    uintptr_t pageAddr = (adrp_addr & 0xFFFFFFFFFFFFF000) + offsetFromPage;
    // Extracting the offset from the target page address to the pointer's address from the next instruction
    uint32_t offsetFromPageStartInst = *reinterpret_cast<uint32_t*>(adrp_addr + ldr_offset);
    uint32_t offsetFromPageStart = ((offsetFromPageStartInst >> 10) & 0xFFF) << (((offsetFromPageStartInst) >> 30) & 0b11);
    
    return pageAddr + offsetFromPageStart;
}

}  // namespace utils
}  // namespace rd