#pragma once

#include "Hook.h"

namespace rd {
namespace sys {

inline int32_t *ScrWork = nullptr;
inline uint32_t *OPTmenuModePtr = nullptr;
inline uint32_t *OPTmenuCur = nullptr;
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

DECLARE_HOOK(ChkViewDic, bool, uint param_1, uint param_2);

DECLARE_HOOK(OptionDefault, void, void);

void Init();

}  // namespace sys
}  // namespace rd