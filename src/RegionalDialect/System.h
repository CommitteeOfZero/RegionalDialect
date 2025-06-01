#pragma once

#include "Hook.h"

namespace rd {
namespace sys {

#pragma pack(4)
struct ScriptThreadState {
  /* 0000 */ int accumulator;
  /* 0004 */ char gap4[16];
  /* 0014 */ unsigned int thread_group_id;
  /* 0018 */ unsigned int sleep_timeout;
  /* 001C */ char gap28[8];
  /* 0024 */ unsigned int loop_counter;
  /* 0028 */ unsigned int loop_target_label_id;
  /* 002C */ unsigned int call_stack_depth;
  /* 0030 */ unsigned int ret_address_ids[8];
  /* 0050 */ unsigned int ret_address_script_buffer_ids[8];
  /* 0070 */ int thread_id;
  /* 0074 */ int script_buffer_id;
  /* 0078 */ char gap120[68];
  /* 00BC */ int thread_local_variables[32];
  /* 013C */ int somePageNumber;
  /* 0140 */ ScriptThreadState *next_context;
  /* 0144 */ ScriptThreadState *prev_context;
  /* 0148 */ ScriptThreadState *next_free_context;
  /* 014C */ void *pc;
};

inline int32_t *ScrWork = nullptr;
inline uint32_t *OPTmenuModePtr = nullptr;
inline uint8_t *OPTmenuCur = nullptr;
inline uint32_t *OPTmenuPagePtr = nullptr;
inline uint32_t *PADcustom = nullptr;
inline uint32_t *PADrefPtr = nullptr;
inline uint32_t *PADonePtr = nullptr;
inline uint32_t *SYSSEvolPtr = nullptr;

DECLARE_HOOK(GSLflatRectF, void,
            int textureId, float spriteX, float spriteY,
            float spriteWidth, float spriteHeight, float displayX,
            float displayY, int color, int opacity, int unk);

DECLARE_HOOK(SetFlag, void, uint flag, uint setValue);

DECLARE_HOOK(GetFlag, bool, uint flag);

DECLARE_HOOK(SpeakerDrawingFunction, void,
            float param1, float param2, float param3, float param4, float param5,
            float param6, int param7,   int param8,   uint param9,  int param10);

DECLARE_HOOK(OptionDispChip2, void, uint param_1);

DECLARE_HOOK(OptionMain, void, void);

DECLARE_HOOK(SSEvolume, void, uint param_1);

DECLARE_HOOK(SSEplay, void, int param_1, int param_2);

void Init();

}  // namespace sys
}  // namespace rd