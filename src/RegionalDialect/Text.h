#pragma once

#include <cstddef>

#include "Hook.h"

#define MAX_PROCESSED_STRING_LENGTH 2000
#define GLYPH_ID_FULLWIDTH_SPACE 63
#define GLYPH_ID_HALFWIDTH_SPACE 0
#define NOT_A_LINK 0xFF

namespace rd {
namespace text {

struct MesFontColor_t {
    uint32_t textColor;
    uint32_t outlineColor;
};

inline uint *MesNameDispLen = nullptr;
inline uint32_t *EPmaxPtr = nullptr;
inline uint32_t *MEStextDatNumPtr = nullptr;
inline uint32_t *MESngFontListTopNumPtr = nullptr;
inline uint32_t *MESngFontListLastNumPtr = nullptr;
inline uint8_t *MEStextFl = nullptr;
inline unsigned short *MEStext = nullptr;
inline unsigned short *MESngFontListLast = nullptr;
inline unsigned short *MESngFontListTop = nullptr;
inline MesFontColor_t *MesFontColor = nullptr;
inline uint32_t *MESrevLineBufUsePtr = nullptr;
inline uint32_t *MESrevDispLinePos = nullptr;
inline uint32_t *MESrevLineBufp = nullptr;
inline unsigned short *MESrevText = nullptr;
inline uint32_t *MESrevDispLinePosY = nullptr;
inline uint8_t *MESrevTextSize = nullptr;
inline unsigned short *MESrevTextPos = nullptr;
inline uint32_t *MESrevDispPosPtr = nullptr;
inline uint32_t *MESrevDispMaxPtr = nullptr;
inline uint8_t ourTable[8000] = { 0 };

DECLARE_HOOK(GSLfontStretchF, int,
            int fontSurfaceId,
            float uv_x, float uv_y, float uv_w, float uv_h,
            float pos_x0, float pos_y0, float pos_x1, float pos_y1,
            uint color, int opacity, bool shrink);

DECLARE_HOOK(GSLfontStretchWithMaskF, int,
            int fontSurfaceId, int maskSurfaceId,
            float uv_x, float uv_y, float uv_w, float uv_h,
            float pos_x0, float pos_y0, float pos_x1, float pos_y1,
            uint color, int opacity);

DECLARE_HOOK(GSLfontStretchWithMaskExF, int,
            int fontSurfaceId, int maskSurfaceId,
            float uv_x, float uv_y, float uv_w, float uv_h,
            float mask_x, float mask_y,
            float pos_x0, float pos_y0, float pos_x1, float pos_y1,
            uint color, int opacity);

DECLARE_HOOK(TipsDataInit, void, ulong thread, unsigned short *addr1, unsigned short *addr2);

DECLARE_HOOK(MESsetNGflag, void, int nameNewline, int rubyEnabled);

DECLARE_HOOK(ChatLayout, int, uint a1, std::byte *a2, uint a3);

DECLARE_HOOK(ChatRendering, void,
            int64_t a1, float a2, float a3, float a4,
            std::byte *a5, unsigned int a6, unsigned int a7,
            float a8, float a9, unsigned int a11);

DECLARE_HOOK(MESdrawTextExF, void,
            int param_1, int param_2, int param_3, uint param_4, int8_t *param_5,
            uint param_6, int param_7, uint param_8, uint param_9);

DECLARE_HOOK(MESrevDispInit, void, void);

DECLARE_HOOK(MESrevDispText, void,
            int fontSurfaceId, int maskSurfaceId, int param3, int param4,
            int param5, int param6, int param7);

DECLARE_HOOK(MEStvramDrawEx, void,
            int param_1, ulong param_2, int param_3, int param_4, int param_5);

void Init(std::string const &romMount);

}  // namespace text
}  // namespace rd
