#include <cstdint>
#include <concepts>

#include "Vm.h"
#include "System.h"
#include "Mem.h"

namespace rd {
namespace vm {

using VmInstruction = void (*)(ScriptThreadState*);

static VmInstruction *SCRuser1 = nullptr;
static VmInstruction *SCRgraph = nullptr;
static VmInstruction *SCRsystem = nullptr;

static inline void PopOpcode(ScriptThreadState *thread) {
    thread->pc = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(thread->pc) + 2);
}

template <std::integral T>
[[maybe_unused]] static inline T Pop(ScriptThreadState *thread)  {
    T ret = *static_cast<T*>(thread->pc);
    thread->pc = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(thread->pc) + sizeof(T));
    return ret;
}

static inline int32_t PopExpr(ScriptThreadState *thread) {
    int32_t ret;
    CalMain::Callback(thread, &ret);
    return ret;
}

void GetDic(ScriptThreadState *thread) {
    PopOpcode(thread);
    
    // For parity with PC
    Pop<uint8_t>(thread);

    auto tipId = PopExpr(thread);
    auto outFlag = PopExpr(thread);

    rd::sys::SetFlag::Callback(outFlag, !rd::sys::ChkViewDic::Callback(tipId, 0));
}

static uintptr_t SlotToPtr(int table, int opcode) {
    switch (table & 0x7F) {
        case 0x00:
            if (SCRsystem == nullptr) break;
            return reinterpret_cast<uintptr_t>(&SCRsystem[opcode]);
        case 0x01:
            if (SCRgraph == nullptr) break;
            return reinterpret_cast<uintptr_t>(&SCRgraph[opcode]);
        case 0x10:
            if (SCRuser1 == nullptr) break;
            return reinterpret_cast<uintptr_t>(&SCRuser1[opcode]);
        default:
            Logging.Log("Table number 0x%02X is invalid!\n", table);
            break;
    }

    return 0;
}

static void InsertCustomInstruction(const std::string &name) {
    if (!rd::config::config["patchdef"]["base"]["customInstructions"].has(name)) return;

    auto inst = rd::config::config["patchdef"]["base"]["customInstructions"][name];

    int table = inst["table"].get<int>();
    int opcode = inst["opcode"].get<int>();

    uintptr_t address = SlotToPtr(table, opcode);
    if (address == 0) return;

    if (*reinterpret_cast<uint32_t*>(address) != 0 &&                       // Non-empty slot
        **reinterpret_cast<uint32_t**>(address) != inst::Ret().Value()) {   // Not a dummy instruction
        Logging.Log("%s cannot be inserted into slot %02X %02X: "
                    "Possibly overwriting existing instruction!",
                    name.c_str(), table, opcode);
        return;
    }

    rd::mem::Overwrite(address, reinterpret_cast<uintptr_t>(&GetDic));
    Logging.Log("%s inserted at %02X %02X!", name.c_str(), table, opcode);
}

void CalMain::Callback(ScriptThreadState *param_1, int32_t *param2) {
    Orig(param_1, param2);
}

void Init() {
    HOOK_VAR(game, SCRuser1);
    HOOK_VAR(game, SCRgraph);
    HOOK_VAR(game, SCRsystem);

    HOOK_FUNC(game, CalMain);

    InsertCustomInstruction("GetDic");
}

}  // namespace vm
}  // namespace rd