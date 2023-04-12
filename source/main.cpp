#include <cmath>

#include "main.hpp"

#include "skyline/logger/TcpLogger.hpp"
#include "skyline/logger/StdoutLogger.hpp"
#include "skyline/utils/ipc.hpp"
#include "skyline/utils/utils.h"

#include "FindPattern.h"
#include "LanguageBarrierStructs.h"

// For handling exceptions
char ALIGNA(0x1000) exception_handler_stack[0x4000];
nn::os::UserExceptionInfo exception_info;

std::string RomMountPath;

void exception_handler(nn::os::UserExceptionInfo* info) {
    skyline::logger::s_Instance->LogFormat("Exception occurred!\n");

    skyline::logger::s_Instance->LogFormat("Error description: %x\n", info->ErrorDescription);
    for (int i = 0; i < 29; i++)
        skyline::logger::s_Instance->LogFormat("X[%02i]: %" PRIx64 "\n", i, info->CpuRegisters[i].x);
    skyline::logger::s_Instance->LogFormat("FP: %" PRIx64 "\n", info->FP.x);
    skyline::logger::s_Instance->LogFormat("LR: %" PRIx64 "\n", info->LR.x);
    skyline::logger::s_Instance->LogFormat("SP: %" PRIx64 "\n", info->SP.x);
    skyline::logger::s_Instance->LogFormat("PC: %" PRIx64 "\n", info->PC.x);
    skyline::logger::s_Instance->Flush();
}

uintptr_t codeCaves;

extern "C" {
    uintptr_t englishTipsHideBranch1;
    uintptr_t englishTipsShowBranch1;
    uintptr_t englishTipsHideBranch2;
    uintptr_t englishTipsShowBranch2;
    void englishTipsBranchFix1(void);
    void englishTipsBranchFix2(void);
}

uint32_t get_u32(uintptr_t address) {
    return *(uint32_t *)address;
}

uintptr_t get_ptr(uintptr_t address) {
    return *(uintptr_t*)address;
}

#define __flush_cache(c, n) __builtin___clear_cache(reinterpret_cast<char*>(c), reinterpret_cast<char*>(c) + n)

void overwrite_u32(uintptr_t address, uint32_t value) {
    skyline::inlinehook::ControlledPages control((void *)address, sizeof(uint32_t));
    control.claim();

    uint32_t *rw = (uint32_t *)control.rw;
    *rw = value;
    __flush_cache((void *)address, sizeof(uint32_t));

    control.unclaim();
}

void overwrite_ptr(uintptr_t address, void *value) {
    skyline::inlinehook::ControlledPages control((void *)address, sizeof(void *));
    control.claim();

    void **rw = (void **)control.rw;
    *rw = value;
    __flush_cache((void *)address, sizeof(void *));

    control.unclaim();
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

uintptr_t retrievePointer(uint64_t adrp_addr, uint64_t ldr_offset = 0x4) {
    uint32_t pageAddrOffsetInst = get_u32(adrp_addr);
    skyline::logger::s_Instance->LogFormat("pageAddrOffsetInst: 0x%x\n", pageAddrOffsetInst);
    // Extracting the offset from the current page to the pointer's page from the instruction
    // immhi:immlo * 4096
    uint64_t immhi = (pageAddrOffsetInst >> 5) & 0x1FFF;
    uint64_t immlo = (pageAddrOffsetInst >> 29) & 0b11;
    uint64_t offsetFromPage = ((immhi << 2) | immlo) << 12;
    // Adding offset to current page address to get target page address
    uintptr_t pageAddr = (adrp_addr & 0xFFFFFFFFFFFFF000) + offsetFromPage;
    // Extracting the offset from the target page address to the pointer's address from the next instruction
    uint32_t offsetFromPageStartInst = get_u32(adrp_addr + ldr_offset);
    skyline::logger::s_Instance->LogFormat("offsetFromPageStartInst: 0x%x\n", offsetFromPageStartInst);
    uint32_t offsetFromPageStart = ((offsetFromPageStartInst >> 10) & 0xFFF) << (((offsetFromPageStartInst) >> 30) & 0b11);
    
    return get_ptr(pageAddr + offsetFromPageStart);
}

using GSLfontStretchFFunc = Result(
    int surfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity, bool shrink
);

using GSLfontStretchWithMaskFFunc = Result(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
);

using GSLfontStretchWithMaskExFFunc = Result(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float mask_x, float mask_y,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
);

using TipsDataInitFunc = Result(ulong thread, ushort *addr1, ushort *addr2);

using MESsetNGflagFunc = Result(int nameNewline, int rubyEnabled);

using ChatLayoutFunc = Result(uint a1, int8_t *a2, uint a3);

using calMainFunc = Result(long param_1, int32_t *param2);

using ChatRenderingFunc = Result(int64_t a1, float a2, float a3, float a4,
                                 char* a5, unsigned int a6, unsigned int a7,
                                 float a8, float a9, unsigned int a11);

using MESdrawTextExFFunc = Result(int param_1, int param_2, int param_3, uint param_4, int8_t *param_5,
                                  uint param_6, int param_7,uint param_8, uint param_9);

using GSLflatRectFFunc = Result(int textureId, float spriteX, float spriteY,
                                float spriteWidth, float spriteHeight, float displayX,
                                float displayY, int color, int opacity, int unk);

using SetFlagFunc = Result(uint flag, uint setValue);

using MainMenuChangesFunc = Result(void);

GSLfontStretchFFunc *GSLfontStretchFImpl;
GSLfontStretchWithMaskFFunc *GSLfontStretchWithMaskFImpl;
GSLfontStretchWithMaskExFFunc *GSLfontStretchWithMaskExFImpl;
TipsDataInitFunc *TipsDataInitImpl;
MESsetNGflagFunc *MESsetNGflagImpl;
ChatLayoutFunc *ChatLayoutImpl;
calMainFunc *calMainImpl;
ChatRenderingFunc *ChatRenderingImpl;
MESdrawTextExFFunc *MESdrawTextExFImpl;
GSLflatRectFFunc *GSLflatRectFImpl;

int handleGSLfontStretchF(
    int fontSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity, bool shrink
) {
    transformFontAtlasCoordinates(
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    int rc;
    rc = GSLfontStretchFImpl(
        fontSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity, shrink
    );
    return rc;
}

int handleGSLfontStretchWithMaskF(
    int fontSurfaceId, int maskSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity
) {
    transformFontAtlasCoordinates(
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1
    );

    int rc;
    rc = GSLfontStretchWithMaskFImpl(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
    return rc;
}

int handleGSLfontStretchWithMaskExF(
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

    int rc;
    rc = GSLfontStretchWithMaskExFImpl(
        fontSurfaceId, maskSurfaceId,
        uv_x, uv_y, uv_w, uv_h,
        mask_x, mask_y,
        pos_x0, pos_y0, pos_x1, pos_y1,
        color, opacity
    );
    return rc;
}

uintptr_t EPmaxPtr;
uint32_t EPmax;

void handleTipsDataInit(ulong thread, ushort *addr1, ushort *addr2) {
    // Running hooked function to populate EPmax
    TipsDataInitImpl(thread, addr1, addr2);

    const char *SystemMenuDispPattern = "EA0F1CFCE923016DFD7B02A9FD830091F44f03A9F31A00F0";

    // cmp and mov instructions with register and immediate value fields zeroed out
    uint32_t cmpiTemplate = 0x7100001F;
    uint32_t moviTemplate = 0x52800000;

    // Retrieve populated EPmax
    EPmax = get_u32(EPmaxPtr);
    skyline::logger::s_Instance->LogFormat("EPmax: %d", EPmax);

    uintptr_t SystemMenuDispAddr = FindPattern(
        (unsigned char*)skyline::utils::g_MainTextAddr,
        (unsigned char*)skyline::utils::g_MainRodataAddr,
        SystemMenuDispPattern,
        skyline::utils::g_MainTextAddr,
        0,
        0);

    // Get address of first comparison to patch
    uintptr_t patchInCmp1Addr = SystemMenuDispAddr - 0x1530;

    // Patching the comparison with the actual EPmax instead of hardcoded value
    // EPmax - 5 because of repeated TIPs
    overwrite_u32(patchInCmp1Addr,      cmpiTemplate | (EPmax - 5 << 10) | (0x9 << 5));
    overwrite_u32(patchInCmp1Addr + 4,  moviTemplate | (EPmax - 5 << 5)  | 0x8);

    // Same for the second comparison, although with different order and registers
    uintptr_t patchInCmp2Addr = patchInCmp1Addr + 0x300;
        
    overwrite_u32(patchInCmp2Addr,      moviTemplate | (EPmax - 5 << 5)  | 0x9);
    overwrite_u32(patchInCmp2Addr + 8,  cmpiTemplate | (EPmax - 5 << 10) | (0x8 << 5));

    overwrite_trampoline(
        englishTipsShowBranch1 - 0x4,
        (uintptr_t)&englishTipsBranchFix1,
        0
    );
    overwrite_trampoline(
        englishTipsShowBranch2 - 0x4,
        (uintptr_t)&englishTipsBranchFix2,
        0
    );
}

uintptr_t MEStextDatNumPtr;
uintptr_t MESngFontListTopNumPtr;
uintptr_t MESngFontListLastNumPtr;
uintptr_t MEStextFlPtr;
uintptr_t MEStextPtr;
uintptr_t MESngFontListLastPtr;
uintptr_t MESngFontListTopPtr;

void handleMESsetNGflag(int nameNewline, int rubyEnabled) {

    uint32_t MEStextDatNum = get_u32(MEStextDatNumPtr);
    uint32_t MESngFontListTopNum = get_u32(MESngFontListTopNumPtr);
    uint32_t MESngFontListLastNum = get_u32(MESngFontListLastNumPtr);
    auto MEStextFl = (char *)(void *)MEStextFlPtr;
    auto MEStext = (unsigned short *)(void *)MEStextPtr;
    auto MESngFontListLast = (unsigned short *)(void *)MESngFontListLastPtr;
    auto MESngFontListTop = (unsigned short *)(void *)MESngFontListTopPtr;

    auto isNGtop = [MESngFontListTopNum, MESngFontListTop](unsigned short glyph)->int {
        for (int i = 0; i < MESngFontListTopNum; i++) {
            if (MESngFontListTop[i] == glyph)
                    return 1;
        }
        return 0;
    };

    auto isNGlast = [MESngFontListLastNum, MESngFontListLast](unsigned short glyph)->int {
        for (int i = 0; i < MESngFontListLastNum; i++) {
            if (MESngFontListLast[i] == glyph)
                return 1;
        }
        return 0;
    };

    auto isLetter = [&isNGtop, &isNGlast](unsigned short glyph)->int {
        return glyph < 0x8000 && !isNGtop(glyph) && !isNGlast(glyph);
    };

    auto nextWord = [MEStextDatNum, &MEStext, &MEStextFl, &isLetter](int &pos)->void {
        int wordLen = 0;
        while (pos < MEStextDatNum && isLetter(MEStext[pos])) {
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

	while (pos < MEStextDatNum) {
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

    for (int i = 0; i < MEStextDatNum; i++) {
        if (MEStextFl[i] == 0x0B || MEStextFl[i] == 0x09) {
            lastLetter = i;
        }
    }
    for (int i = lastLetter; i < MEStextDatNum; i++) {
        if (MEStextFl[i] != 0x07) {
            MEStextFl[i] = 0x0B;
        }
    }

}

void handlecalMain(long param_1, int32_t *param2) {
    calMainImpl(param_1, param2);
}

char ourTable[8000] = {0};
char const *ourHankaku = "";
char const *ourZenkaku = "";

uintptr_t ScrWorkPtr;
uintptr_t MesFontColorPtr;

void semiTokeniseSc3String(int8_t *sc3string, std::list<StringWord_t> &words,
                           int baseGlyphSize, int lineLength) {
    ScriptThreadState sc3;
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
                handlecalMain((long)(void *)&sc3, &sc3evalResult);
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

    auto ScrWork = (int32_t *)(void *)ScrWorkPtr;
    auto MesFontColor = (uint8_t *)(void *)MesFontColorPtr;

    ScriptThreadState sc3;
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
                    handlecalMain((long)(void *)&sc3, &sc3evalResult);
                    sc3string = (int8_t *)sc3.pc;
                    int scrWorkColor;

                    if (sc3evalResult == 255) {
                        scrWorkColor = ScrWork[2166];
                        sc3evalResult = scrWorkColor;
                    }
                    if (sc3evalResult == 254) {
                        scrWorkColor = ScrWork[2167];
                        sc3evalResult = scrWorkColor;
                    }
                    if (sc3evalResult == 253) {
                        scrWorkColor = ScrWork[2168];
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

int handleChatLayout(uint a1, int8_t *a2, uint a3) {
    ProcessedSc3String_t str;
    std::list<StringWord_t> words;

    float glyphSize = a3 * 1.1f;
    semiTokeniseSc3String(a2, words, glyphSize, a1);
    processSc3TokenList(0, 0, a1, words, 255, 20, glyphSize, &str,
                        false, 1.5f, -1, NOT_A_LINK, glyphSize, 25);
    
    if (str.lines == 0) return 1;
    return str.lines;
}

void handleChatRendering(int64_t a1, float a2, float a3, float a4,
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
            handleGSLfontStretchF(93, str.textureStartX[i], str.textureStartY[i],
                                  str.textureWidth[i], str.textureHeight[i],
                                  str.displayStartX[i], str.displayStartY[i],
                                  str.displayEndX[i], str.displayEndY[i],
                                  curColor, a11 / 2, false);
    }
}

void handleMESdrawTextExF(int param_1, int param_2, int param_3, uint param_4, int8_t *param_5,
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

void handleGSLflatRectF(int textureId, float spriteX, float spriteY,
                        float spriteWidth, float spriteHeight, float displayX,
                        float displayY, int color, int opacity, int unk) {
    const int promptOffset = 100;

    if (textureId == 80 && displayX == 1651 && displayY == 988 &&
        spriteHeight == 42 && spriteWidth == 42) {
        displayX += promptOffset;
    } else if (textureId == 80 && displayX == 1703 && displayY == 988 &&
               spriteHeight == 42 && spriteWidth == 42) {
        displayX += promptOffset;
    } else if (textureId == 80 && displayX == 1755 && displayY == 988 &&
               spriteHeight == 42 && spriteWidth == 42) {
        displayX += promptOffset;
    } else if (textureId == 155 && spriteX == 1247 && spriteY == 1086 && 
               spriteWidth == 23 && spriteHeight == 122 && displayX == 1799 &&
               displayY > 854) {
        displayY = 854;
    }
    GSLflatRectFImpl(textureId, spriteX, spriteY, spriteWidth,
                     spriteHeight, displayX, displayY, color,
                     opacity, unk);
}

void loadWidths() {
    Result rc = 0;
    rc = skyline::utils::readFile(RomMountPath + "system/widths.bin", 0, &ourTable[0], 8000);
    if (R_SUCCEEDED(rc)){
        skyline::logger::s_Instance->Log("Successfully loaded widths\n");
        return;
    } else
        skyline::logger::s_Instance->LogFormat("Failed to load widths: 0x%x\n", rc);
}

static skyline::utils::Task* after_romfs_task = new skyline::utils::Task{[]() {
    loadWidths();
}};

void stub() {
}


Result (*nnFsMountRomImpl)(char const*, void*, unsigned long);
Result handleNnFsMountRom(char const* path, void* buffer, unsigned long size) {
    Result rc = 0;
    rc = nnFsMountRomImpl(path, buffer, size);

    if (R_SUCCEEDED(rc))
        skyline::logger::s_Instance->Log("Successfully mounted ROM\n");
    else
        skyline::logger::s_Instance->LogFormat("Failed to mount rom. (0x%x)\n", rc);
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();

    RomMountPath = std::string(path) + ":/";

    // start task queue
    skyline::utils::SafeTaskQueue* taskQueue = new skyline::utils::SafeTaskQueue(100);
    //taskQueue->startThread(20, 3, 0x4000);
    taskQueue->startThread(20, -2, 0x4000);
    taskQueue->push(new std::unique_ptr<skyline::utils::Task>(after_romfs_task));
    nn::os::WaitEvent(&after_romfs_task->completionEvent);

    return rc;
}

void (*VAbortImpl)(char const*, char const*, char const*, int, Result const*, nn::os::UserExceptionInfo const*, char const*, va_list args);
void handleNnDiagDetailVAbort(char const* str1, char const* str2, char const* str3, int int1, Result const* code, nn::os::UserExceptionInfo const* ExceptionInfo, char const* fmt, va_list args) {
    int len = vsnprintf(nullptr, 0, fmt, args);
    char* fmt_info = new char[len + 1];
    vsprintf(fmt_info, fmt, args);

    const char* fmt_str = "%s\n%s\n%s\n%d\nError: 0x%x\n%s";
    len = snprintf(nullptr, 0, fmt_str, str1, str2, str3, int1, *code, fmt_info);
    char* report = new char[len + 1];
    sprintf(report, fmt_str, str1, str2, str3, int1, *code, fmt_info);

    skyline::logger::s_Instance->LogFormat("%s", report);
    nn::err::ApplicationErrorArg* error =
        new nn::err::ApplicationErrorArg(69, "The software is aborting.", report,
                                         nn::settings::LanguageCode::Make(nn::settings::Language::Language_English));
    nn::err::ShowApplicationError(*error);
    delete[] report;
    delete[] fmt_info;
    VAbortImpl(str1, str2, str3, int1, code, ExceptionInfo, fmt, args);
}

void skyline_main() {
    // populate our own process handle
    Handle h;
    skyline::utils::Ipc::getOwnProcessHandle(&h);
    envSetOwnProcessHandle(h);

    // init hooking setup
    A64HookInit();

    // initialize logger
    skyline::logger::s_Instance = new skyline::logger::StdoutLogger();
    skyline::logger::s_Instance->Log("[skyline_main] Beginning initialization.\n");
    skyline::logger::s_Instance->StartThread();

    // override exception handler to dump info
    nn::os::SetUserExceptionHandler(exception_handler, exception_handler_stack, sizeof(exception_handler_stack),
                                    &exception_info);

    // hook to prevent the game from double mounting romfs
    A64HookFunction(reinterpret_cast<void*>(nn::fs::MountRom), reinterpret_cast<void*>(handleNnFsMountRom),
                    (void**)&nnFsMountRomImpl);

    // manually init nn::ro ourselves, then stub it so the game doesn't try again
    nn::ro::Initialize();
    A64HookFunction(reinterpret_cast<void*>(nn::ro::Initialize), reinterpret_cast<void*>(stub), NULL);

    for (int i = 0; i < 8000; i++)
        ourTable[i] = 32;

    uintptr_t code = skyline::utils::g_MainTextAddr;
    const char *GSLfontStretchFPattern =                    "FF0301D1FD7B03A9FDC30091295C00120AE0BF";
    const char *GSLfontStretchWithMaskFPattern =            "FF0303D1FD7B09A9FD430291F55300F9F4";
    const char *GSLfontStretchWithMaskExFPattern =          "FF0303D1FD7B09A9FD430291F55300F9F44F0BA9B03B40BDA61F";
    const char *fontAlinePattern =                          "20111111";
    const char *audioLoweringPattern =                      "187F0153";
    const std::vector<const char *> widthCheckPatterns =    {"1F790571", "3F790571", "3F7D0571", "5F790571", "7F790571", "7F3D0671", "BF3D0671", "7F7D0571", "5F3D0671"};
    // const char *TipsInitPattern =                           "FD7BBAA9FC6F01A9FD030091FA6702A9F85F03A9F65704A9F44F05A9FF3B40D1FF833CD1";
    const char *TipsDataInitPattern =                       "FD7BBAA9FC6F01A9FD030091FA6702A9F85F03A9F65704A9F44F05A9FF3B40D1FFC329";
    // const char *MESsetNGflagPattern =                       "F85FBDA9F65701A9F44F02A9881A00B0087546F908";
    const char *MESsetNGflagPattern =                       "F85FBDA9F65701A9F44F02A9881A00D008C546F908";
    const char *ChatLayoutPattern =                         "FF4307D1FD7B17A9FDC30591FC6F18A9FA6719A9F85F1AA9F6571BA9F44F1CA92B";
    // const char *calMainPattern =                          "FF4302D1FD7B03A9FDC30091FC6F04A9FA6705A9F85F06A9F65707A9F44F08A9771B00D0F7";
    const char *calMainPattern =                            "FF4302D1FD7B03A9FDC30091FC6F04A9FA6705A9F85F06A9F65707A9F44F08A9771B00F0F7";
    const char *Noah_8DPattern =                            "FD7BBEA9F30B00F9FD030091206281522100805233008052????????081C00B0084D47F9080140F909AC8D520801098B090140B9290500113F8100";
    const char *MESsetTvramPattern =                        "FD7BBAA9FC6F01A9FA6702A9F85F03A9F65704A9F44F05A9A81A00B008C546";
    const char *ChatRenderingPattern =                      "EF3BB66DED33016DEB2B026DE923036DFD7B04A9FD030191FC6F05A9FA6706A9F85F07A9F65708A9F44F09A9FF0740D1FF0306";
    const char *MESdrawTextExFPattern =                     "E80F19FCFD7B01A9FD430091FC6F02A9FA6703A9F85F04A9F65705A9F44F06A9FF0740D1";
    const char *GSLflatRectFPattern =                       "FF4301D1FD7B03A9FDC30091F44F04A94820";
    const char *SaveMenuGuidePattern =                      "010000001027";

    uintptr_t MESsetNGflagAddr = FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, MESsetNGflagPattern, code, 0, 0);
    uintptr_t Noah_8DAddr = FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, Noah_8DPattern, code, 0, 0);
    uintptr_t MESsetTvramAddr = FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, MESsetTvramPattern, code, 0, 0);
    uintptr_t TipsDataInitAddr = FindPattern((unsigned char*)code, (unsigned char *)skyline::utils::g_MainRodataAddr, TipsDataInitPattern, code, 0, 0);

    codeCaves = skyline::utils::g_MainRodataAddr - 0xC30;
    englishTipsHideBranch1 = code + 0x248a8;
    englishTipsShowBranch1 = code + 0x248c0;
    englishTipsHideBranch2 = code + 0x2499c;
    englishTipsShowBranch2 = code + 0x249b4;

    MEStextDatNumPtr = retrievePointer(MESsetNGflagAddr + 28, -0xC);
    MESngFontListTopNumPtr = retrievePointer(MESsetNGflagAddr + 28, 0x8);
    MESngFontListLastNumPtr = retrievePointer(MESsetNGflagAddr + 32, 0x8);
    MEStextFlPtr = retrievePointer(MESsetNGflagAddr + 48, 0xC);
    MEStextPtr = retrievePointer(MESsetNGflagAddr + 52, 0x10);
    MESngFontListLastPtr = retrievePointer (MESsetNGflagAddr + 56, 0x20);
    MESngFontListTopPtr = retrievePointer(MESsetNGflagAddr + 64, 0x24);
    ScrWorkPtr = retrievePointer(Noah_8DAddr + 28);
    MesFontColorPtr = retrievePointer(MESsetTvramAddr + 84, 0x18);
    EPmaxPtr = retrievePointer(TipsDataInitAddr + 0x154);

    uintptr_t fontAlineAddr = FindPattern((unsigned char*)skyline::utils::g_MainDataAddr, (unsigned char*)skyline::utils::g_MainBssAddr, fontAlinePattern, skyline::utils::g_MainDataAddr, 0, 0);
    char fontAlineAddrStr[9];
    sprintf(fontAlineAddrStr, "%08X", __builtin_bswap32(fontAlineAddr));
    uintptr_t fontAlinePtr = FindPattern((unsigned char*)skyline::utils::g_MainDataAddr, (unsigned char *)skyline::utils::g_MainBssAddr, fontAlineAddrStr, skyline::utils::g_MainDataAddr, 0, 0);

    uintptr_t fontAline2Addr = FindPattern((unsigned char*)skyline::utils::g_MainDataAddr, (unsigned char*)skyline::utils::g_MainBssAddr, fontAlinePattern, skyline::utils::g_MainDataAddr, 0, 1);
    char fontAline2AddrStr[9];
    sprintf(fontAline2AddrStr, "%08X", __builtin_bswap32(fontAline2Addr));
    uintptr_t fontAline2Ptr = FindPattern((unsigned char*)skyline::utils::g_MainDataAddr, (unsigned char *)skyline::utils::g_MainBssAddr, fontAline2AddrStr, skyline::utils::g_MainDataAddr, 0, 0);

    auto SaveMenuGuide = (uint32_t *)(void *)FindPattern((unsigned char*)skyline::utils::g_MainDataAddr, (unsigned char*)skyline::utils::g_MainBssAddr, SaveMenuGuidePattern, skyline::utils::g_MainDataAddr, 0, 0);

    std::vector<uint32_t> buttonIds;

    buttonIds.push_back(0);
    buttonIds.push_back(10010);
    buttonIds.push_back(2);
    buttonIds.push_back(10020);
    buttonIds.push_back(6);
    buttonIds.push_back(7);
    buttonIds.push_back(10100);
    buttonIds.push_back(1);
    buttonIds.push_back(10000);
    buttonIds.push_back(255);

    memcpy(&SaveMenuGuide[6 * 20], buttonIds.data(), buttonIds.size() * sizeof(uint32_t));

    uintptr_t audioLoweringAddr = FindPattern((unsigned char *)code, (unsigned char *)skyline::utils::g_MainRodataAddr, audioLoweringPattern, skyline::utils::g_MainDataAddr, 0, 0);
    uint32_t branchFix = 0x3A5F43E8;
    uint32_t nop = 0xD503201F;
   
    int occur = 0;
    for (const char *pattern : widthCheckPatterns) {
        while (true) {
            uintptr_t tentative = FindPattern((unsigned char *)code, (unsigned char *)skyline::utils::g_MainRodataAddr, pattern, code, 0, occur);
            if (tentative == 0) break;
            overwrite_u32(tentative, branchFix);
            occur++;
        }
        occur = 0;
    }

    //  fontAline
    overwrite_ptr(fontAlinePtr, &ourTable[0]);
    // fontAline2
    overwrite_ptr(fontAline2Ptr, &ourTable[0]);

    uintptr_t englishOnlyOffsetTable1 = FindPattern((unsigned char*)(fontAline2Addr + 400), (unsigned char*)(fontAline2Addr + 400 + 8), "FFFFFF00", fontAline2Addr + 400, 0, 0);
    if (englishOnlyOffsetTable1 == 0) {
        for (int i = 0; i <= 80 * 2; i++) {
            overwrite_u32(fontAline2Addr + 400 + (i * 4), 0x00000000);
        }
    }

    overwrite_u32(audioLoweringAddr,     nop);
    overwrite_u32(audioLoweringAddr + 4, nop);

    overwrite_u32(code + 0x437a8, 0x17FFFFB9);
    overwrite_u32(code + 0x2bc88, 0x17FFFF39);
    overwrite_u32(code + 0x2baac, 0x17FFFFB0);

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char *)skyline::utils::g_MainRodataAddr, GSLfontStretchFPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleGSLfontStretchF),
        (void **)&GSLfontStretchFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char *)skyline::utils::g_MainRodataAddr, GSLfontStretchWithMaskFPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleGSLfontStretchWithMaskF),
        (void **)&GSLfontStretchWithMaskFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char *)skyline::utils::g_MainRodataAddr, GSLfontStretchWithMaskExFPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleGSLfontStretchWithMaskExF),
        (void **)&GSLfontStretchWithMaskExFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(TipsDataInitAddr),
        reinterpret_cast<void*>(handleTipsDataInit),
        (void **)&TipsDataInitImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(MESsetNGflagAddr),
        reinterpret_cast<void*>(handleMESsetNGflag),
        (void **)&MESsetNGflagImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char *)skyline::utils::g_MainRodataAddr, ChatLayoutPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleChatLayout),
        (void **)&ChatLayoutImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, calMainPattern, code, 0, 0)),
        reinterpret_cast<void*>(handlecalMain),
        (void **)&calMainImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, ChatRenderingPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleChatRendering),
        (void **)&ChatRenderingImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, MESdrawTextExFPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleMESdrawTextExF),
        (void **)&MESdrawTextExFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(FindPattern((unsigned char*)code, (unsigned char*)skyline::utils::g_MainRodataAddr, GSLflatRectFPattern, code, 0, 0)),
        reinterpret_cast<void*>(handleGSLflatRectF),
        (void **)&GSLflatRectFImpl
    );

}   

extern "C" void skyline_init() {
    skyline::utils::init();
    virtmemSetup();  // needed for libnx JIT

    skyline_main();
}
