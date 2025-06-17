#pragma once

#include <cstddef>
#include <span>

#include "Hook.h"

#define MAX_PROCESSED_STRING_LENGTH 2000
#define GLYPH_ID_FULLWIDTH_SPACE 0
#define GLYPH_ID_HALFWIDTH_SPACE 63
#define NOT_A_LINK 0xFF

namespace rd {
namespace text {

typedef struct {
  int lines;
  int length;
  int textureStartX[MAX_PROCESSED_STRING_LENGTH];
  int textureStartY[MAX_PROCESSED_STRING_LENGTH];
  int textureWidth[MAX_PROCESSED_STRING_LENGTH];
  int textureHeight[MAX_PROCESSED_STRING_LENGTH];
  int displayStartX[MAX_PROCESSED_STRING_LENGTH];
  int displayStartY[MAX_PROCESSED_STRING_LENGTH];
  int displayEndX[MAX_PROCESSED_STRING_LENGTH];
  int displayEndY[MAX_PROCESSED_STRING_LENGTH];
  int color[MAX_PROCESSED_STRING_LENGTH];
  int glyph[MAX_PROCESSED_STRING_LENGTH];
  uint8_t linkNumber[MAX_PROCESSED_STRING_LENGTH];
  int linkCharCount;
  int linkCount;
  int curLinkNumber;
  int curColor;
  int usedLineLength;
  bool error;
  char text[MAX_PROCESSED_STRING_LENGTH];
} ProcessedSc3String_t;

typedef struct {
  int8_t* start;
  int8_t* end;
  uint16_t cost;
  bool startsWithSpace;
  bool endsWithLinebreak;
} StringWord_t;

inline uint *MesNameDispLen = nullptr;
inline uint32_t *EPmaxPtr = nullptr;
inline uint32_t *MEStextDatNumPtr = nullptr;
inline uint32_t *MESngFontListTopNumPtr = nullptr;
inline uint32_t *MESngFontListLastNumPtr = nullptr;
inline uint8_t *MEStextFl = nullptr;
inline unsigned short *MEStext = nullptr;
inline unsigned short *MESngFontListLast = nullptr;
inline unsigned short *MESngFontListTop = nullptr;
inline uint8_t *MesFontColor = nullptr;
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

DECLARE_HOOK(ChatLayout, int, uint a1, int8_t *a2, uint a3);

DECLARE_HOOK(ChatRendering, void,
            int64_t a1, float a2, float a3, float a4,
            int8_t* a5, unsigned int a6, unsigned int a7,
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
