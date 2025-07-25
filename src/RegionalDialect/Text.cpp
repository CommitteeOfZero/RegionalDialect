#include <cmath>
#include <list>
#include <vector>
#include <functional>
#include <algorithm>

#include <sys/endian.h>
#include <skyline/utils/cpputils.hpp>
#include <log/logger_mgr.hpp>

#include "Mem.h"
#include "System.h"
#include "Vm.h"
#include "Text.h"

#define DIALOGUE_FONT_SURFACE_ID    91
#define OUTLINE_FONT_SURFACE_ID  /* 93 */ \
    (DIALOGUE_FONT_SURFACE_ID + 2) 

extern "C" {
    void englishTipsBranchFix(void);
}

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
  size_t glyph[MAX_PROCESSED_STRING_LENGTH];
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
  std::byte *start;
  std::byte *end;
  uint16_t cost;
  bool startsWithSpace;
  bool endsWithLinebreak;
} StringWord_t;

// From https://github.com/CommitteeOfZero/impacto/blob/bfc23774eeeb4bcf853cace270ac3ac58eb681f1/src/text.cpp#L38
struct StringTokenType {
    enum value : uint8_t {
        LineBreak = 0x00,
        CharacterNameStart = 0x01,
        DialogueLineStart = 0x02,
        Present = 0x03,
        SetColor = 0x04,
        Present_Clear = 0x08,
        RubyBaseStart = 0x09,
        RubyTextStart = 0x0A,
        RubyTextEnd = 0x0B,
        SetFontSize = 0x0C,
        PrintInParallel = 0x0E,
        CenterText = 0x0F,
        SetTopMargin = 0x11,
        SetLeftMargin = 0x12,
        GetHardcodedValue = 0x13,
        EvaluateExpression = 0x15,
        UnlockTip = 0x16,
        Present_0x18 = 0x18,
        AutoForward = 0x19,
        AutoForward_1A = 0x1A,
        RubyCenterPerCharacter = 0x1E,
        AltLineBreak = 0x1F,

        // This is our own!
        Character = 0xFE,

        EndOfString = 0xFF
    };
};

static float AtlasDialogueMargin = 0.0f;
static float AtlasOutlineMargin = 0.0f;
static float DialogueOutlineOffset = 0.0f;
static bool OutlinedFont = false;

static int CurrentShadowFont = DIALOGUE_FONT_SURFACE_ID;

static bool NametagImplementation = false;

void transformFontAtlasCoordinates(
    int &fontSurfaceId, uint &color,
    float& uv_x, float& uv_y, float& uv_w, float& uv_h,
    float& pos_x0, float& pos_y0, float& pos_x1, float& pos_y1
) {
    
    float margin = 0.0f;

    // Black used for font shadow, so switch to outline font
    if (OutlinedFont && color == 0x00000000u)
        fontSurfaceId = CurrentShadowFont;

    switch (fontSurfaceId) {
        case DIALOGUE_FONT_SURFACE_ID:
            margin = AtlasDialogueMargin;
            break;
        case OUTLINE_FONT_SURFACE_ID:
            margin = OutlinedFont ? AtlasOutlineMargin : AtlasDialogueMargin;
            break;
        default:
            UNREACHABLE;
    }

    if (margin == 0.0f) return;

    if (OutlinedFont && fontSurfaceId == OUTLINE_FONT_SURFACE_ID) {
        for (const auto &coord : std::to_array<std::reference_wrapper<float>>(
            { pos_x0, pos_x1, pos_y0, pos_y1 }))
            coord.get() += DialogueOutlineOffset;
    }  

    const float size = 48.0f;
    const float newSize = size + margin * 2;

    float scale_x = (pos_x1 - pos_x0) / uv_w;
    float scale_y = (pos_y1 - pos_y0) / uv_h;

    float x = std::round(uv_x / size);
    float y = std::round(uv_y / size);

    float origin_x = x * size;
    float origin_y = y * size;

    float left_offset = (origin_x - margin) - uv_x;
    float top_offset = (origin_y - margin) - uv_y;
    float right_offset = (origin_x + size + margin) - (uv_x + uv_w);
    float bottom_offset = (origin_y + size + margin) - (uv_y + uv_h);

    pos_x0 += left_offset * scale_x;
    pos_y0 += top_offset * scale_y;
    pos_x1 += right_offset * scale_x;
    pos_y1 += bottom_offset * scale_y;

    uv_x = x * newSize;
    uv_y = y * newSize;
    uv_w = uv_h = newSize;
}

void semiTokeniseSc3String(std::byte *sc3String, std::list<StringWord_t> &words,
                           int baseGlyphSize, int lineLength) {
    StringWord_t word = { sc3String, NULL, 0, false, false };

    while (sc3String != nullptr) {
        switch (std::to_integer<std::underlying_type_t<StringTokenType::value>>(*sc3String)) {
            case StringTokenType::EndOfString:
                word.end = sc3String - 1;
                words.emplace_back(word);
                return;
            case StringTokenType::LineBreak:
                word.end = sc3String - 1;
                word.endsWithLinebreak = true;
                words.emplace_back(word);
                word = { ++sc3String, NULL, 0, false, false };
                break;
            case StringTokenType::SetColor: {
                rd::vm::ScriptThreadState dummy = { .pc = sc3String + 1 };
                EXL_UNUSED(rd::vm::PopExpr(&dummy));
                sc3String = dummy.pc;
                break;
            }
            case StringTokenType::RubyBaseStart:
            case StringTokenType::RubyTextEnd:
            case StringTokenType::RubyCenterPerCharacter:
            case StringTokenType::AltLineBreak:
                sc3String++;
                break;
            default:
                size_t glyphId = be16dec(sc3String) & 0x7FFF;
                uint16_t glyphWidth = (baseGlyphSize * ourTable[glyphId]) / 32;
                if (glyphId == GLYPH_ID_FULLWIDTH_SPACE || glyphId == GLYPH_ID_HALFWIDTH_SPACE) {
                    word.end = sc3String - 1;
                    words.emplace_back(word);
                    word = {sc3String, NULL, glyphWidth, true, false};
                } else {
                    if (word.cost + glyphWidth > lineLength) {
                        word.end = sc3String - 1;
                        words.emplace_back(word);
                        word = {sc3String, NULL, 0, false, false};
                    }
                    word.cost += glyphWidth;
                }
                sc3String += 2;
                break;
        }
    }
}

void processSc3TokenList(int xOffset, int yOffset,int lineLength,
                        std::list<StringWord_t> &words, int lineCount, int color,
                        int baseGlyphSize, ProcessedSc3String_t *result,
                        bool measureOnly, float multiplier,
                        int lastLinkNumber, int curLinkNumber,
                        int currentColor, int lineHeight) {

    ::memset(result, 0, sizeof(ProcessedSc3String_t));

    int curLineLength = 0;
    int prevLineLength = 0;

    int spaceCost = (baseGlyphSize * ourTable[GLYPH_ID_FULLWIDTH_SPACE]) / 32;

    for (auto it = words.begin(); it != words.end(); it++) {
        if (result->lines >= lineCount) {
            words.erase(words.begin(), it);
            break;
        }
        int wordCost = it->cost - spaceCost * (int)(!curLineLength && it->startsWithSpace);
        if (curLineLength + wordCost > lineLength) {
            wordCost -= spaceCost * (int)(curLineLength && it->startsWithSpace);
            result->lines++;
            prevLineLength = curLineLength;
            curLineLength = 0;
        }
        if (result->lines >= lineCount) {
            words.erase(words.begin(), it);
            break;
        };

        std::byte *sc3String = it->start + (int)(!curLineLength && it->startsWithSpace) * 2;

        while (sc3String <= it->end) {
            switch (std::to_integer<std::underlying_type_t<StringTokenType::value>>(*sc3String)) {
                case StringTokenType::EndOfString:
                    goto afterWord;
                    break;
                case StringTokenType::LineBreak:
                    goto afterWord;
                    break;
                case StringTokenType::SetColor: {
                    rd::vm::ScriptThreadState dummy = { .pc = sc3String + 1 };
                    auto colorIndex = rd::vm::PopExpr(&dummy);
                    sc3String = dummy.pc;

                    if (colorIndex >= 253 && colorIndex <= 255)
                        colorIndex = rd::sys::ScrWork[2166 + (255 - colorIndex)];

                    currentColor = color ?
                        MesFontColor[colorIndex].textColor :
                        MesFontColor[colorIndex].outlineColor;
                    break;
                }
                case StringTokenType::RubyBaseStart:
                    curLinkNumber = ++lastLinkNumber;
                    sc3String++;
                    break;
                case StringTokenType::RubyTextEnd:
                    curLinkNumber = NOT_A_LINK;
                    sc3String++;
                    break;
                case StringTokenType::RubyCenterPerCharacter:
                    sc3String++;
                    [[ fallthrough ]];
                case StringTokenType::AltLineBreak:
                    sc3String++;
                    break;
                default: {
                    size_t glyphId = be16dec(sc3String) & 0x7FFF;
                    int i = result->length;
                    if (result->lines >= lineCount) break;
                    if (curLinkNumber != NOT_A_LINK) {
                        result->linkCharCount++;
                    }
                    uint16_t glyphWidth = (baseGlyphSize * ourTable[glyphId]) / 32;
                    curLineLength += glyphWidth;
                    if (!measureOnly) {
                        result->linkNumber[i] = curLinkNumber;
                        result->glyph[i] = glyphId;
                        result->textureStartX[i] =
                            32 * multiplier * (glyphId % 64);
                        result->textureStartY[i] =
                            32 * multiplier * (glyphId / 64);
                        result->textureWidth[i] = ourTable[glyphId] * multiplier;
                        result->textureHeight[i] = 32 * multiplier;
                        result->displayStartX[i] =
                            (xOffset + (curLineLength - glyphWidth)) * multiplier;
                        result->displayStartY[i] =
                            (yOffset + (result->lines * lineHeight)) * multiplier;
                        result->displayEndX[i] = (xOffset + curLineLength) * multiplier;
                        result->displayEndY[i] =
                            (yOffset + ((result->lines) * lineHeight + baseGlyphSize)) *
                            multiplier;
                        result->color[i] = currentColor;
                    }
                    result->length++;
                    sc3String += 2;
                    break;
                }
            }
        }
    afterWord:
        if (it->endsWithLinebreak) {
            result->lines++;
            prevLineLength = curLineLength;
            curLineLength = 0;
        }
    }

    if (curLineLength == 0) result->lines--;
    if (result->lines > 0) result->lines++;

    result->linkCount = lastLinkNumber + 1;
    result->curColor = currentColor;
    result->curLinkNumber = curLinkNumber;
    result->usedLineLength = curLineLength ? curLineLength : prevLineLength;
}

int GSLfontStretchF::Callback(
    int fontSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity, bool shrink
) {

    if (NametagImplementation &&
        fontSurfaceId == DIALOGUE_FONT_SURFACE_ID &&
        (pos_y0 == 760.5f || pos_y0 == 757.5f)) {
        if (!rd::sys::GetFlag::Callback(801)) return 0;
        float offset = (MesNameDispLen[0] * 1.5f) / 2.0f;
        pos_x0 += offset; pos_x1 += offset;
    }

    transformFontAtlasCoordinates(
        fontSurfaceId, color,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return Orig(
        fontSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity, shrink
    );
}

int GSLfontStretchWithMaskF::Callback(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
) {
    transformFontAtlasCoordinates(
        fontSurfaceId, color,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return Orig(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
}

int GSLfontStretchWithMaskExF::Callback(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float mask_x, float mask_y,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
) {
    transformFontAtlasCoordinates(
        fontSurfaceId, color,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return Orig(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        mask_x, mask_y,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
}

void TipsDataInit::Callback(ulong thread, unsigned short *addr1, unsigned short *addr2) {
    // Running hooked function to populate EPmax
    Orig(thread, addr1, addr2);

    uintptr_t SystemMenuDispAddr = rd::hook::SigScan("game", "SystemMenuDisp");

    // Get address of first comparison to patch
    const uintptr_t patchInCmp1Addr = SystemMenuDispAddr - 0x1530;

    // Patching the comparison with the actual EPmax instead of hardcoded value
    // EPmax - 5 because of repeated TIPs
    rd::mem::Overwrite(patchInCmp1Addr,       inst::CmpImmediate(reg::W9, *EPmaxPtr - 5).Value());
    rd::mem::Overwrite(patchInCmp1Addr + 4,   inst::Movz(reg::W8, *EPmaxPtr - 5).Value());
    
    // Same for the second comparison, although with different order and registers
    const uintptr_t patchInCmp2Addr = patchInCmp1Addr + 0x300;
        
    rd::mem::Overwrite(patchInCmp2Addr,       inst::Movz(reg::W9, *EPmaxPtr - 5).Value());
    rd::mem::Overwrite(patchInCmp2Addr + 8,   inst::CmpImmediate(reg::W8, *EPmaxPtr - 5).Value());
}

void MESsetNGflag::Callback(bool nameNewline, bool rubyEnabled) {

    const auto isNGFunc = [](uint16_t *ptr, uint32_t length) {
        return [ptr, length](unsigned short glyph)->bool {
            return std::find(ptr, ptr + length, glyph) != ptr + length;
        };
    };

    const auto isNGTop = isNGFunc(MESngFontListTop, *MESngFontListTopNumPtr);

    const auto isNGLast = isNGFunc(MESngFontListLast, *MESngFontListLastNumPtr);


    const auto isLetter = [&isNGTop, &isNGLast](unsigned short glyph)->bool {
        return static_cast<int16_t>(glyph) > 0 && !isNGTop(glyph) && !isNGLast(glyph);
    };

    const auto nextWord = [&isLetter](uint32_t &pos)->void {
        int wordLen = 0;
        while (pos < *MEStextDatNumPtr && isLetter(MEStext[pos])) {
            MEStextFl[pos] = wordLen == 0 ? 0x0A : 0x0B;
            pos++; wordLen++;
        }
        MEStextFl[pos - 1] = wordLen == 1 ? 0x00 : 0x09;
    };

    bool processingRuby = false;
    bool processingRubyText = false;
    uint32_t pos = 0;

    if ((MEStext[0] & 0xFF) == StringTokenType::CharacterNameStart) {
        MEStextFl[pos++] = 0x02;
        while ((MEStext[pos] & 0xFF) != StringTokenType::DialogueLineStart)
            MEStextFl[pos++] = 0x0B;
        MEStextFl[pos++] = nameNewline ? 0x07 : 0x01;
    }

    while (pos < *MEStextDatNumPtr) {
        uint16_t glyph = MEStext[pos];

        MEStextFl[pos] = 0;

        if (glyph & 0x8000) {
            switch (glyph & 0xFF) {
                case StringTokenType::LineBreak:
                    MEStextFl[pos] = 0x07;
                    break;
                case StringTokenType::RubyBaseStart:
                    processingRuby = rubyEnabled;
                    MEStextFl[pos] = 0x02;
                    break;
                case StringTokenType::RubyTextStart:
                    processingRubyText = rubyEnabled;
                    MEStextFl[pos] = 0x0B;
                    break;
                case StringTokenType::RubyTextEnd:
                    processingRuby = processingRubyText = false;
                    MEStextFl[pos] = 0x01;
                    break;
                case StringTokenType::SetLeftMargin:
                    MEStextFl[pos] = 0x02;
                    break;
                case StringTokenType::RubyCenterPerCharacter:
                    MEStextFl[pos] = 0x0B;
                    break;
                default:
                    break;
            }
            pos++;
            continue;
        }

        if (processingRubyText || processingRuby) {
            // processingRubyText -> 0x1B
            // processingRuby -> 0x0B
            MEStextFl[pos] = 0x0B | (processingRubyText << 4);
            pos++;
        } else if (uint8_t topLast = isNGTop(glyph) | (isNGLast(glyph) << 1)) {
            // NG top  -> 0x01
            // NG last -> 0x02
            // NG both -> 0x03
            MEStextFl[pos] = topLast;
            pos++;
        } else {
            nextWord(pos);
        }
    }

    uint32_t lastLetter = 0;

    for (uint32_t i = *MEStextDatNumPtr - 1; i >= 0; i--) {
        if (MEStextFl[i] != 0x0B && MEStextFl[i] != 0x09) continue;
        lastLetter = i;
        break;
    }

    for (uint32_t i = lastLetter; i < *MEStextDatNumPtr; i++) {
        if (MEStextFl[i] == 0x07) continue;
        MEStextFl[i] = 0x0B;
    }
}


int ChatLayout::Callback(uint a1, std::byte *a2, uint a3) {
    ProcessedSc3String_t str;
    std::list<StringWord_t> words;

    float glyphSize = a3 * 1.1f;
    semiTokeniseSc3String(a2, words, glyphSize, a1);
    processSc3TokenList(0, 0, a1, words, 255, 20, glyphSize, &str,
                        false, 1.5f, -1, NOT_A_LINK, glyphSize, 25);
    
    if (str.lines == 0) return 1;
    return str.lines;
}

void ChatRendering::Callback(int64_t a1, float a2, float a3, float a4,
                         std::byte* a5, unsigned int a6, unsigned int a7,
                         float a8, float a9, unsigned int a11) {

    if (a7 == 0x808080 && a8 == 18) return;
    ProcessedSc3String_t str;
    std::list<StringWord_t> words;
    a11 *= 1.75f;
    float glyphSize = a8 * 1.1f;

    semiTokeniseSc3String(a5, words, glyphSize, a4);
    processSc3TokenList(a2, a3, a4, words, 255, a7, glyphSize, &str,
                        false, 1.5f, -1, NOT_A_LINK, a7, glyphSize);

    for (int i = 0; i < str.length; i++) {
        int curColor = str.color[i];

        if (str.textureWidth[i] > 0 && str.textureHeight[i] > 0)
            GSLfontStretchF::Callback(93, str.textureStartX[i], str.textureStartY[i],
                                  str.textureWidth[i], str.textureHeight[i],
                                  str.displayStartX[i], str.displayStartY[i],
                                  str.displayEndX[i], str.displayEndY[i],
                                  curColor, a11 / 2, false);
    }
}

void MESdrawTextExF::Callback(int param_1, int param_2, int param_3, uint param_4, int8_t *param_5,
                          uint param_6, int param_7, uint param_8, uint param_9) {
    if (param_4 == 0x164 && param_8 == 0x15) {
        if (param_7 == 0x808080) return;
        if (param_3 == 0x96 || param_3 == 0x117 || param_3 == 0x198 || param_3 == 0x21A) {
            char year[8];
            char month[4];
            char day[4];
            char date[20];
            auto sc3 = (char*)param_5;
            memcpy(year, sc3, 8);
            sc3 += 10;
            if (*(sc3 + 1) == 0x3F) *(sc3 + 1) = 0x01;
            memcpy(month, sc3, 4);
            sc3 += 6;
            if (*(sc3 + 1) == 0x3F) *(sc3 + 1) = 0x01;
            memcpy(day, sc3, 4);
            memcpy(date, month, 4);
            date[4] = 0x80; date[5] = 0x40;
            memcpy(date + 6, day, 4);
            date[10] = 0x80; date[11] = 0x40;
            memcpy(date + 12, year, 8);
            memcpy(param_5, date, 20);
        }
    } else if ((param_2 == 0xD2 || param_2 == 0xD3) && param_4 == 0x1C8 && param_7 == 0x5C3AB4 && param_8 == 0x14) {
        param_2 += 8;
    } else if ((param_2 == 0x29C || param_2 == 0x29D) && param_4 == 0x134 && param_7 == 0x5C3AB4 && param_8 == 0x14) {
        param_2 += 8;
    } else if ((param_2 == 0xC2 || param_2 == 0xC3) && param_4 == 0x500 && param_7 == 0x5C3AB4 && param_8 == 0x14) {
        param_2 += 24;
    } else if ((param_2 == 0x72 || param_2 == 0x73) && param_4 == 0x500 && param_7 == 0x5C3AB4 && param_8 == 0x14) {
        param_2 += 1;
    }
    Orig(param_1, param_2, param_3, param_4, param_5,
                       param_6, param_7, param_8, param_9);
}

void MESrevDispInit::Callback(void) {
    Orig();
    
    if (!rd::sys::GetFlag::Callback(801)) return;

    int voicedCount = 0;

    for (uint32_t i = 0; i < *MESrevLineBufUsePtr; i++) {
        if (reinterpret_cast<short*>(MESrevText)[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            *MESrevDispMaxPtr += 45; voicedCount++;
        }
        
        MESrevDispLinePosY[i] += 45 * voicedCount;
    }

    // Underflow workaround
    *MESrevDispPosPtr = std::max<uint32_t>(*MESrevDispMaxPtr, 506) - 506;
}

void MESrevDispText::Callback(int fontSurfaceId, int maskSurfaceId, int param3, int param4,
                          int param5, int param6, int param7) {

    if (!rd::sys::GetFlag::Callback(801)) {
        Orig(fontSurfaceId, maskSurfaceId, param3, param4, param5, param6, param7);
        return;
    }

    for (uint32_t i = 0; i < *MESrevLineBufUsePtr; i++) {
        if ((short)MESrevText[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            uint32_t iVar5 = (MESrevDispLinePosY[i] + param4) - *MESrevDispPosPtr - 30;
            uint32_t widthAccum = 150;
            for (size_t nametagIndex = MESrevLineBufp[MESrevDispLinePos[i]] + 1;
                reinterpret_cast<short*>(MESrevText)[nametagIndex] > 0;
                nametagIndex++) {
            
                uint32_t iVar15 = iVar5 + MESrevTextPos[nametagIndex << 1 | 1];
                uint32_t currWidth = (28 * ourTable[MESrevText[nametagIndex]] / 32) * 1.5f;

                GSLfontStretchWithMaskF::Callback(
                    fontSurfaceId,
                    maskSurfaceId,
                    ((MESrevText[nametagIndex] & 0x3f) << 5) * 1.5f,
                    ((MESrevText[nametagIndex] >> 1) & 0x7fe0) * 1.5f,
                    MESrevTextSize[nametagIndex << 2] * 1.5f,
                    MESrevTextSize[(nametagIndex << 2) | 1] * 1.5f,
                    widthAccum,
                    iVar15 * 1.5f,
                    widthAccum + currWidth,
                    (iVar15 + (32 * MESrevTextSize[(nametagIndex << 2) | 3] / 28) * 1.1f) * 1.5f,
                    0,
                    param7
                );
                
                widthAccum += currWidth;
            }
        }
    }

    Orig(fontSurfaceId, maskSurfaceId, param3, param4, param5, param6, param7);
}

void MEStvramDrawEx::Callback(int param_1, ulong param_2, int param_3, int param_4, int param_5) {
    CurrentShadowFont = OUTLINE_FONT_SURFACE_ID;
    Orig(param_1, param_2, param_3, param_4, param_5);
    CurrentShadowFont = DIALOGUE_FONT_SURFACE_ID;
}

void Init(std::string const &romMount) {
    Result rc = 0;
    rc = skyline::utils::readFile(romMount + "system/widths.bin", 0, &ourTable[0], 8000);
    if (R_SUCCEEDED(rc)) {
        Logging.Log("Successfully loaded widths\n");
    } else {
        Logging.Log("Failed to load widths: 0x%x\n", rc);
    }

    HOOK_VAR(game, MesNameDispLen);
    HOOK_VAR(game, EPmaxPtr);
    HOOK_VAR(game, MEStextDatNumPtr);
    HOOK_VAR(game, MESngFontListTopNumPtr);
    HOOK_VAR(game, MESngFontListLastNumPtr);
    HOOK_VAR(game, MEStextFl);
    HOOK_VAR(game, MEStext);
    HOOK_VAR(game, MESngFontListLast);
    HOOK_VAR(game, MESngFontListTop);
    HOOK_VAR(game, MesFontColor);
    HOOK_VAR(game, MESrevLineBufUsePtr);
    HOOK_VAR(game, MESrevDispLinePos);
    HOOK_VAR(game, MESrevLineBufp);
    HOOK_VAR(game, MESrevText);
    HOOK_VAR(game, MESrevDispLinePosY);
    HOOK_VAR(game, MESrevTextSize);
    HOOK_VAR(game, MESrevTextPos);
    HOOK_VAR(game, MESrevDispPosPtr);
    HOOK_VAR(game, MESrevDispMaxPtr);

    if (rd::config::config["patchdef"]["base"]["hookText"].get<bool>()) {
        uintptr_t englishOnlyOffsetTable = rd::hook::SigScan("game", "englishOnlyOffsetTable");
        if (englishOnlyOffsetTable != 0 && *(uint32_t*)englishOnlyOffsetTable != 0xFFFFFF00)
            ::memset(reinterpret_cast<void*>(englishOnlyOffsetTable), 0, 640);

        if (rd::config::config["gamedef"]["signatures"]["game"].has("widthCheck")) {
            uint32_t branchFix = 0x3A5F43E8; 
            for (uintptr_t widthCheck : rd::hook::SigScanArray("game", "widthCheck", true))
                rd::mem::Overwrite(widthCheck, branchFix);
        }

        if (rd::config::config["gamedef"]["signatures"]["game"].has("fontAlinePtr"))
            rd::mem::Overwrite(rd::hook::SigScan("game", "fontAlinePtr"), &ourTable[0]);

        if (rd::config::config["gamedef"]["signatures"]["game"].has("fontAline2Ptr"))
            rd::mem::Overwrite(rd::hook::SigScan("game", "fontAline2Ptr"), &ourTable[0]);

        AtlasDialogueMargin = rd::config::config["patchdef"]["base"]["atlasDialogueMargin"].get<float>();
        AtlasOutlineMargin = rd::config::config["patchdef"]["base"]["atlasOutlineMargin"].get<float>();
        DialogueOutlineOffset = rd::config::config["patchdef"]["base"]["dialogueOutlineOffset"].get<float>();

        if (rd::config::config["patchdef"]["base"]["outlinedFont"].get<bool>()) {
            OutlinedFont = true;
            HOOK_FUNC(game, MEStvramDrawEx);
        }

        HOOK_FUNC(game, GSLfontStretchF);
        HOOK_FUNC(game, GSLfontStretchWithMaskF);
        HOOK_FUNC(game, GSLfontStretchWithMaskExF);
        HOOK_FUNC(game, MESsetNGflag);
    }

    if (rd::config::config["patchdef"]["base"]["tipReimplementation"].get<bool>()) {
        rd::mem::Trampoline(
            rd::hook::SigScan("game", "englishTipsFixBranch1"),
            (uintptr_t)&englishTipsBranchFix,
            reg::X0
        );

        rd::mem::Trampoline(
            rd::hook::SigScan("game", "englishTipsFixBranch2"),
            (uintptr_t)&englishTipsBranchFix,
            reg::X0
        );

        HOOK_FUNC(game, TipsDataInit);
    }

    if (rd::config::config["patchdef"]["base"]["chnRedoChat"].get<bool>()) {
        HOOK_FUNC(game, ChatLayout);
        HOOK_FUNC(game, ChatRendering);
    }

    HOOK_FUNC(game, MESdrawTextExF);

    if (rd::config::config["patchdef"]["base"]["addNametags"].get<bool>()) {
        NametagImplementation = true;
        HOOK_FUNC(game, MESrevDispInit);
        HOOK_FUNC(game, MESrevDispText);
    }
}

}  // namespace text
}  // namespace rd