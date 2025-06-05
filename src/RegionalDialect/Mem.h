#pragma once

#include <cstdint>
#include <cstring>

#include <lib/armv8.hpp>
#include <util/sys/rw_pages.hpp>

namespace inst = exl::armv8::inst;
namespace reg = exl::armv8::reg;

namespace rd {
namespace mem {

template <typename T>
inline void Overwrite(uintptr_t address, const T &value) {
    static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable!");
    exl::util::RwPages control(address, sizeof(T));
    ::memcpy(reinterpret_cast<void*>(control.GetRw()), &value, sizeof(T));
    control.Flush();
}

void Trampoline(uintptr_t address, uintptr_t target, reg::Register reg);

uintptr_t AssemblePointer(uintptr_t adrp_addr, ptrdiff_t ldr_offset);

}  // namespace mem
}  // namespace rd