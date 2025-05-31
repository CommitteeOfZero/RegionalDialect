#include <math.h>
#include <list>
#include <vector>

#include "skyline/utils/cpputils.hpp"

#include "RegionalDialect/System.h"
#include "RegionalDialect/Utils.h"

#define TEXT_IMPLEMENTATION
#include "RegionalDialect/Text.h"

extern "C" {
    uintptr_t englishTipsHideBranch1;
    uintptr_t englishTipsShowBranch1;
    uintptr_t englishTipsHideBranch2;
    uintptr_t englishTipsShowBranch2;
    void englishTipsBranchFix1(void);
    void englishTipsBranchFix2(void);
}

extern std::string RomMountPath;

namespace rd {
namespace text {

void transformFontAtlasCoordinates(
    float& uv_x, float& uv_y, float& uv_w, float& uv_h,
    float& pos_x0, float& pos_y0, float& pos_x1, float& pos_y1
) {
    const float margin = 8;
    const float size = 48;
    const float newSize = 48 + margin*2;

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

void semiTokeniseSc3String(int8_t *sc3string, std::list<StringWord_t> &words,
                           int baseGlyphSize, int lineLength) {
    rd::sys::ScriptThreadState sc3;
    int sc3evalResult;
    StringWord_t word = {sc3string, NULL, 0, false, false};
    int8_t c;
    while (sc3string != nullptr) {
        c = *sc3string;
        switch (c) {
            case -1:
                word.end = sc3string - 1;
                words.push_back(word);
                return;
            case 0:
                word.end = sc3string - 1;
                word.endsWithLinebreak = true;
                words.push_back(word);
                word = {++sc3string, NULL, 0, false, false};
                break;
            case 4:
                sc3.pc = sc3string + 1;
                HandleCalMain((long)(void *)&sc3, &sc3evalResult);
                sc3string = (int8_t *)sc3.pc;
                break;
            case 9:
            case 0xB:
            case 0x1E:
            case 0x1F:
                sc3string++;
                break;
            default:
                int glyphId = (uint8_t)sc3string[1] + ((c & 0x7F) << 8);
                uint16_t glyphWidth = (baseGlyphSize * ourTable[glyphId]) / 32;
                if (glyphId == GLYPH_ID_FULLWIDTH_SPACE ||
                    glyphId == GLYPH_ID_HALFWIDTH_SPACE) {
                    word.end = sc3string - 1;
                    words.push_back(word);
                    word = {sc3string, NULL, glyphWidth, true, false};
                } else {
                    if (word.cost + glyphWidth > lineLength) {
                        word.end = sc3string - 1;
                        words.push_back(word);
                        word = {sc3string, NULL, 0, false, false};
                    }
                    word.cost += glyphWidth;
                }
                sc3string += 2;
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

    rd::sys::ScriptThreadState sc3;
    int sc3evalResult;

    memset(result, 0, sizeof(ProcessedSc3String_t));

    int curLineLength = 0;
    int prevLineLength = 0;

    int spaceCost = (baseGlyphSize * ourTable[GLYPH_ID_FULLWIDTH_SPACE]) / 32;

    for (auto it = words.begin(); it != words.end(); it++) {
        if (result->lines >= lineCount) {
            words.erase(words.begin(), it);
            break;
        }
        int wordCost =
            it->cost -
            ((curLineLength == 0 && it->startsWithSpace == true) ? spaceCost : 0);
        if (curLineLength + wordCost > lineLength) {
            if (curLineLength != 0 && it->startsWithSpace == true)
                wordCost -= spaceCost;
            result->lines++;
            prevLineLength = curLineLength;
            curLineLength = 0;
        }
        if (result->lines >= lineCount) {
            words.erase(words.begin(), it);
            break;
        };

        int8_t c;
        int8_t *sc3string = (curLineLength == 0 && it->startsWithSpace == true)
                            ? it->start + 2
                            : it->start;
        while (sc3string <= it->end) {
            c = *sc3string;
            switch (c) {
                case -1:
                    goto afterWord;
                    break;
                case 0:
                    goto afterWord;
                    break;
                case 4:
                    sc3.pc = sc3string + 1;
                    HandleCalMain((long)(void *)&sc3, &sc3evalResult);
                    sc3string = (int8_t *)sc3.pc;
                    int scrWorkColor;

                    if (sc3evalResult == 255) {
                        scrWorkColor = rd::sys::ScrWork[2166];
                        sc3evalResult = scrWorkColor;
                    }
                    if (sc3evalResult == 254) {
                        scrWorkColor = rd::sys::ScrWork[2167];
                        sc3evalResult = scrWorkColor;
                    }
                    if (sc3evalResult == 253) {
                        scrWorkColor = rd::sys::ScrWork[2168];
                        sc3evalResult = scrWorkColor;
                    }

                    if (color)
                        currentColor = MesFontColor[2 * sc3evalResult];
                    else
                        currentColor = MesFontColor[2 * sc3evalResult + 1];
                    break;
                case 9:
                    curLinkNumber = ++lastLinkNumber;
                    sc3string++;
                    break;
                case 0xB:
                    curLinkNumber = NOT_A_LINK;
                    sc3string++;
                    break;
                case 0x1E:
                    sc3string++;
                case 0x1F:
                    sc3string++;
                    break;
                default:
                    int glyphId = (uint8_t)sc3string[1] + ((c & 0x7F) << 8);
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
                    sc3string += 2;
                    break;
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


int HandleGSLfontStretchF(
    int fontSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity, bool shrink
) {

    if (fontSurfaceId == 91 && (pos_y0 == 760.5f || pos_y0 == 757.5f)) {
        if (!rd::sys::HandleGetFlag(801)) return 0;
        float offset = (MesNameDispLen[0] * 1.5f) / 2.0f;
        pos_x0 += offset; pos_x1 += offset;
    }

    transformFontAtlasCoordinates(
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return GSLfontStretchFImpl(
        fontSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity, shrink
    );
}

int HandleGSLfontStretchWithMaskF(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
) {
    transformFontAtlasCoordinates(
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return GSLfontStretchWithMaskFImpl(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
}

int HandleGSLfontStretchWithMaskExF(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float mask_x, float mask_y,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
) {
    transformFontAtlasCoordinates(
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    return GSLfontStretchWithMaskExFImpl(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        mask_x, mask_y,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
}

void HandleTipsDataInit(ulong thread, unsigned short *addr1, unsigned short *addr2) {
    // Running hooked function to populate EPmax
    TipsDataInitImpl(thread, addr1, addr2);

    // cmp and mov instructions with register and immediate value fields zeroed out
    const uint32_t cmpiTemplate = 0x7100001F;
    const uint32_t moviTemplate = 0x52800000;

    uintptr_t SystemMenuDispAddr = rd::hook::SigScan("game", "SystemMenuDisp");

    // Get address of first comparison to patch
    const uintptr_t patchInCmp1Addr = SystemMenuDispAddr - 0x1530;

    // Patching the comparison with the actual EPmax instead of hardcoded value
    // EPmax - 5 because of repeated TIPs
    rd::utils::overwrite_u32(patchInCmp1Addr,      cmpiTemplate | ((*EPmaxPtr - 5) << 10) | (0x9 << 5));
    rd::utils::overwrite_u32(patchInCmp1Addr + 4,  moviTemplate | ((*EPmaxPtr - 5) << 5)  | 0x8);

    // Same for the second comparison, although with different order and registers
    const uintptr_t patchInCmp2Addr = patchInCmp1Addr + 0x300;
        
    rd::utils::overwrite_u32(patchInCmp2Addr,      moviTemplate | ((*EPmaxPtr - 5) << 5)  | 0x9);
    rd::utils::overwrite_u32(patchInCmp2Addr + 8,  cmpiTemplate | ((*EPmaxPtr - 5) << 10) | (0x8 << 5));

    rd::utils::overwrite_trampoline(
        englishTipsShowBranch1 - 0x4,
        (uintptr_t)&englishTipsBranchFix1,
        0
    );
    rd::utils::overwrite_trampoline(
        englishTipsShowBranch2 - 0x4,
        (uintptr_t)&englishTipsBranchFix2,
        0
    );
}

void HandleMESsetNGflag(int nameNewline, int rubyEnabled) {

    auto isNGtop = [](unsigned short glyph)->bool {
        for (int i = 0; i < *MESngFontListTopNumPtr; i++) {
            if (MESngFontListTop[i] == glyph)
                    return true;
        }
        return false;
    };

    auto isNGlast = [](unsigned short glyph)->bool {
        for (int i = 0; i < *MESngFontListLastNumPtr; i++) {
            if (MESngFontListLast[i] == glyph)
                return true;
        }
        return false;
    };

    auto isLetter = [&isNGtop, &isNGlast](unsigned short glyph)->bool {
        return glyph < 0x8000 && !isNGtop(glyph) && !isNGlast(glyph);
    };

    auto nextWord = [&isLetter](int &pos)->void {
        int wordLen = 0;
        while (pos < *MEStextDatNumPtr && isLetter(MEStext[pos])) {
            MEStextFl[pos] = wordLen == 0 ? 0x0A : 0x0B;
            pos++; wordLen++;
        }
        MEStextFl[pos - 1] = wordLen == 1 ? 0x00 : 0x09;
    };

	int processingRuby = 0;
	int processingRubyText = 0;
    int pos = 0;

    const uint16_t nameStart = 0x8001;
    const uint16_t nameEnd = 0x8002;

    if (MEStext[0] == nameStart) {
        MEStextFl[pos++] = 0x02;
        while (MEStext[pos] != nameEnd) {
            MEStextFl[pos] = 0x0B;
            pos++;
        }
        MEStextFl[pos++] = nameNewline ? 0x07 : 0x01;
    }

	while (pos < *MEStextDatNumPtr) {
		unsigned short glyph = MEStext[pos];

		MEStextFl[pos] = 0;

		if (glyph & 0x8000) {
			switch (glyph & 0xff) {
				case 0x00:
					MEStextFl[pos] = 0x07;
					break;
				case 0x09:
					processingRuby = rubyEnabled;
					MEStextFl[pos] = 0x02;
					break;
				case 0x0a:
					processingRubyText = rubyEnabled;
                    MEStextFl[pos] = 0x0B;
					break;
				case 0x0b:
					processingRuby = 0;
					processingRubyText = 0;
					MEStextFl[pos] = 0x01;
					break;
				case 0x12:
					MEStextFl[pos] = 0x02;
					break;
				case 0x1e:
					MEStextFl[pos] = 0x0B;
					break;
			}
            pos++;
            continue;
		}

		if (processingRubyText) {
			MEStextFl[pos] = 0x1B;
			pos++;
		} else if (processingRuby) {
			MEStextFl[pos] = 0x0b;
			pos++;
		} else if (isNGtop(glyph) && isNGlast(glyph)) {
            MEStextFl[pos] = 0x01 | 0x02;
            pos++;
        } else if (isNGtop(glyph)) {
            MEStextFl[pos] = 0x01;
            pos++;
        } else if (isNGlast(glyph)) {
            MEStextFl[pos] = 0x02;
            pos++;
        } else {
            nextWord(pos);
        }
	}

    int lastLetter = 0;

    for (int i = 0; i < *MEStextDatNumPtr; i++) {
        if (MEStextFl[i] == 0x0B || MEStextFl[i] == 0x09) {
            lastLetter = i;
        }
    }
    for (int i = lastLetter; i < *MEStextDatNumPtr; i++) {
        if (MEStextFl[i] != 0x07) {
            MEStextFl[i] = 0x0B;
        }
    }
}


int HandleChatLayout(uint a1, int8_t *a2, uint a3) {
    ProcessedSc3String_t str;
    std::list<StringWord_t> words;

    float glyphSize = a3 * 1.1f;
    semiTokeniseSc3String(a2, words, glyphSize, a1);
    processSc3TokenList(0, 0, a1, words, 255, 20, glyphSize, &str,
                        false, 1.5f, -1, NOT_A_LINK, glyphSize, 25);
    
    if (str.lines == 0) return 1;
    return str.lines;
}

void HandleCalMain(long param_1, int32_t *param2) {
    CalMainImpl(param_1, param2);
}

void HandleChatRendering(int64_t a1, float a2, float a3, float a4,
                         int8_t* a5, unsigned int a6, unsigned int a7,
                         float a8, float a9, unsigned int a11) {

    if (a7 == 0x808080 && a8 == 18) return;
    ProcessedSc3String_t str;
    std::list<StringWord_t> words;
    a11 *= 1.75;
    float glyphSize = a8 * 1.1f;

    semiTokeniseSc3String(a5, words, glyphSize, a4);
    processSc3TokenList(a2, a3, a4, words, 255, a7, glyphSize, &str,
                        false, 1.5f, -1, NOT_A_LINK, a7, glyphSize);

    for (int i = 0; i < str.length; i++) {
        int curColor = str.color[i];

        if (str.textureWidth[i] > 0 && str.textureHeight[i] > 0)
            HandleGSLfontStretchF(93, str.textureStartX[i], str.textureStartY[i],
                                  str.textureWidth[i], str.textureHeight[i],
                                  str.displayStartX[i], str.displayStartY[i],
                                  str.displayEndX[i], str.displayEndY[i],
                                  curColor, a11 / 2, false);
    }
}

void HandleMESdrawTextExF(int param_1, int param_2, int param_3, uint param_4, int8_t *param_5,
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
    MESdrawTextExFImpl(param_1, param_2, param_3, param_4, param_5,
                       param_6, param_7, param_8, param_9);
}

void HandleMESrevDispInit(void) {
    MESrevDispInitImpl();
    
    if (!rd::sys::HandleGetFlag(801)) return;

    for (uint32_t i = 0; i < *MESrevLineBufUsePtr; i++) {
        if ((short)MESrevText[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            *MESrevDispMaxPtr += 30; *MESrevDispPosPtr += 30;
            for (uint32_t j = i; j < *MESrevLineBufUsePtr; j++) MESrevDispLinePosY[j] += 30;
        }
    }
}

void HandleMESrevDispText(int fontSurfaceId, int maskSurfaceId, int param3, int param4,
                          int param5, int param6, int param7) {

    if (!rd::sys::HandleGetFlag(801)) {
        MESrevDispTextImpl(fontSurfaceId, maskSurfaceId, param3, param4, param5, param6, param7);
        return;
    }

    for (uint32_t i = 0; i < *MESrevLineBufUsePtr; i++) {
        if ((short)MESrevText[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            uint32_t iVar5 = (MESrevDispLinePosY[i] + param4) - *MESrevDispPosPtr - 17;
            uint32_t widthAccum = 150;
            for (uint nametagIndex = MESrevLineBufp[MESrevDispLinePos[i]] + 1;
                (short)MESrevText[nametagIndex] > 0;
                nametagIndex++) {
            
                uint32_t iVar15 = iVar5 + *(short*)((long)MESrevTextPos + (ulong)(nametagIndex << 1 | 1) * 2);
                uint32_t currWidth = (28 * ourTable[MESrevText[nametagIndex]] / 32) * 1.5f;

                HandleGSLfontStretchWithMaskF(
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

    MESrevDispTextImpl(fontSurfaceId, maskSurfaceId, param3, param4, param5, param6, param7);
}


void Init() {
    Result rc = 0;
    rc = skyline::utils::readFile(RomMountPath + "system/widths.bin", 0, &ourTable[0], 8000);
    if (R_SUCCEEDED(rc)) {
        skyline::logger::s_Instance->Log("Successfully loaded widths\n");
    } else {
        skyline::logger::s_Instance->LogFormat("Failed to load widths: 0x%x\n", rc);
    }

    MesNameDispLen = (uint*)rd::hook::SigScan("game", "MesNameDispLen");
    EPmaxPtr = (uint32_t*)rd::hook::SigScan("game", "EPmaxPtr");
    MEStextDatNumPtr = (uint32_t*)rd::hook::SigScan("game", "MEStextDatNumPtr");
    MESngFontListTopNumPtr = (uint32_t*)rd::hook::SigScan("game", "MESngFontListTopNumPtr");
    MESngFontListLastNumPtr = (uint32_t*)rd::hook::SigScan("game", "MESngFontListLastNumPtr");
    MEStextFl = (uint8_t*)rd::hook::SigScan("game", "MEStextFl");
    MEStext = (unsigned short*)rd::hook::SigScan("game", "MEStext");
    MESngFontListLast = (unsigned short*)rd::hook::SigScan("game", "MESngFontListLast");
    MESngFontListTop = (unsigned short*)rd::hook::SigScan("game", "MESngFontListTop");
    MesFontColor = (uint8_t*)rd::hook::SigScan("game", "MesFontColor");
    MESrevLineBufUsePtr = (uint32_t*)rd::hook::SigScan("game", "MESrevLineBufUsePtr");
    MESrevDispLinePos = (uint32_t*)rd::hook::SigScan("game", "MESrevDispLinePos");
    MESrevLineBufp = (uint32_t*)rd::hook::SigScan("game", "MESrevLineBufp");
    MESrevText = (unsigned short*)rd::hook::SigScan("game", "MESrevText");
    MESrevDispLinePosY = (uint32_t*)rd::hook::SigScan("game", "MESrevDispLinePosY");
    MESrevTextSize = (uint8_t*)rd::hook::SigScan("game", "MESrevTextSize");
    MESrevTextPos = (uint32_t*)rd::hook::SigScan("game", "MESrevTextPos");
    MESrevDispPosPtr = (uint32_t*)rd::hook::SigScan("game", "MESrevDispPosPtr");
    MESrevDispMaxPtr = (uint32_t*)rd::hook::SigScan("game", "MESrevDispMaxPtr");

    englishTipsHideBranch1 = rd::hook::SigScan("game", "englishTipsHideBranch1");
    englishTipsShowBranch1 = rd::hook::SigScan("game", "englishTipsShowBranch1");
    englishTipsHideBranch2 = rd::hook::SigScan("game", "englishTipsHideBranch2");
    englishTipsShowBranch2 = rd::hook::SigScan("game", "englishTipsShowBranch2");

    uintptr_t englishOnlyOffsetTable = rd::hook::SigScan("game", "englishOnlyOffsetTable");
    if (englishOnlyOffsetTable != 0 && *(uint32_t*)englishOnlyOffsetTable != 0xFFFFFF00)
        rd::utils::memset(englishOnlyOffsetTable, 0, 640);

    if (rd::config::config["gamedef"].has("widthCheckPatterns")) {
        const std::vector<char *> widthCheckPatterns = rd::config::config["gamedef"]["widthCheckPatterns"].get<std::vector<char*>>();
        uint32_t branchFix = 0x3A5F43E8; 

        int occur = 0;
        for (const char *pattern : widthCheckPatterns) {
            while (true) {
                uintptr_t tentative =
                    rd::hook::FindPattern((unsigned char *)(uintptr_t)skyline::utils::g_MainTextAddr, (unsigned char *)skyline::utils::g_MainRodataAddr, pattern, (uintptr_t)skyline::utils::g_MainTextAddr, 0, occur);
                if (tentative == 0) break;
                rd::utils::overwrite_u32(tentative, branchFix);
                occur++;
            }
            occur = 0;
        }
    }

    if (rd::config::config["gamedef"]["signatures"]["game"].has("fontAlinePtr"))
        rd::utils::overwrite_ptr(rd::hook::SigScan("game", "fontAlinePtr"), &ourTable[0]);

    if (rd::config::config["gamedef"]["signatures"]["game"].has("fontAline2Ptr"))
        rd::utils::overwrite_ptr(rd::hook::SigScan("game", "fontAline2Ptr"), &ourTable[0]);

    HOOK_FUNC(game, GSLfontStretchF);
    HOOK_FUNC(game, GSLfontStretchWithMaskF);
    HOOK_FUNC(game, GSLfontStretchWithMaskExF);
    HOOK_FUNC(game, TipsDataInit);
    HOOK_FUNC(game, MESsetNGflag);
    HOOK_FUNC(game, ChatLayout);
    HOOK_FUNC(game, CalMain);
    HOOK_FUNC(game, ChatRendering);
    HOOK_FUNC(game, MESdrawTextExF);
    HOOK_FUNC(game, MESrevDispInit);
    HOOK_FUNC(game, MESrevDispText);
}

}  // namespace text
}  // namespace rd