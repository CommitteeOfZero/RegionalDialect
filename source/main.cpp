#include <cmath>

#include "main.hpp"

#include "skyline/logger/TcpLogger.hpp"
#include "skyline/logger/StdoutLogger.hpp"
#include "skyline/utils/ipc.hpp"
#include "skyline/utils/utils.h"

#include "LanguageBarrierStructs.h"
#include "Config.h"
#include "SigScan.h"

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
using GetFlagFunc = Result(uint flag);

using MainMenuChangesFunc = Result(void);

using MESrevDispInitFunc = Result(void);

using MESrevDispTextFunc = Result(int fontSurfaceId, int maskSurfaceId, int param3, int param4,
                                  int param5, int param6, int param7);

using SpeakerDrawingFunctionFunc = Result(float param1, float param2, float param3, float param4, float param5,
                                          float param6, int param7,   int param8,   uint param9, int param10);

using OptionDispChip2Func = Result(uint param_1);

using OptionMainFunc = Result(void);

using SSEvolumeFunc = Result(uint param_1);

using SSEplayFunc = Result(int param_1, int param_2);

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
SetFlagFunc *SetFlagImpl;
GetFlagFunc *GetFlagImpl;
MESrevDispInitFunc *MESrevDispInitImpl;
MESrevDispTextFunc *MESrevDispTextImpl;
SpeakerDrawingFunctionFunc *SpeakerDrawingFunctionImpl;
OptionDispChip2Func *OptionDispChip2Impl;
OptionMainFunc *OptionMainImpl;
SSEvolumeFunc *SSEvolumeImpl;
SSEplayFunc *SSEplayImpl;

bool handleGetFlag(uint flag) {
    return GetFlagImpl(flag);
}

void handleSetFlag(uint flag, uint setValue) {
    SetFlagImpl(flag, setValue);
}

uintptr_t MesNameDispLenPtr;

int handleGSLfontStretchF(
    int fontSurfaceId,
    float uv_x, float uv_y, float uv_w, float uv_h,
    float pos_x0, float pos_y0, float pos_x1, float pos_y1,
    uint color, int opacity, bool shrink
) {

    if (fontSurfaceId == 91 && (pos_y0 == 760.5f || pos_y0 == 757.5f)) {
        if (!handleGetFlag(801)) return 0;
        uint *MesNameDispLen = (uint*)(void*)(MesNameDispLenPtr);
        pos_x0 += (MesNameDispLen[0] * 1.5f) / 2.0f;
        pos_x1 += (MesNameDispLen[0] * 1.5f) / 2.0f;;
    }

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

    // cmp and mov instructions with register and immediate value fields zeroed out
    uint32_t cmpiTemplate = 0x7100001F;
    uint32_t moviTemplate = 0x52800000;

    // Retrieve populated EPmax
    EPmax = get_u32(EPmaxPtr);

    uintptr_t SystemMenuDispAddr = sigScan("game", "SystemMenuDisp");

    // Get address of first comparison to patch
    uintptr_t patchInCmp1Addr = SystemMenuDispAddr - 0x1530;

    // Patching the comparison with the actual EPmax instead of hardcoded value
    // EPmax - 5 because of repeated TIPs
    overwrite_u32(patchInCmp1Addr,      cmpiTemplate | ((EPmax - 5) << 10) | (0x9 << 5));
    overwrite_u32(patchInCmp1Addr + 4,  moviTemplate | ((EPmax - 5) << 5)  | 0x8);

    // Same for the second comparison, although with different order and registers
    uintptr_t patchInCmp2Addr = patchInCmp1Addr + 0x300;
        
    overwrite_u32(patchInCmp2Addr,      moviTemplate | ((EPmax - 5) << 5)  | 0x9);
    overwrite_u32(patchInCmp2Addr + 8,  cmpiTemplate | ((EPmax - 5) << 10) | (0x8 << 5));

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

uintptr_t MESrevLineBufUsePtr;
uintptr_t MESrevTextPtr;
uintptr_t MESrevLineBufpPtr;
uintptr_t MESrevDispLinePosPtr;
uintptr_t MESrevDispLinePosYPtr;
uintptr_t MESrevLineBufSizePtr;
uintptr_t MESrevDispMaxPtr;
uintptr_t MESrevTextSizePtr;
uintptr_t MESrevTextPosPtr;
uintptr_t MESrevDispPosPtr;
uintptr_t MESrevTextColPtr;
uintptr_t fontDrawModePtr;

bool shown = false;

void handleMESrevDispInit(void) {
    MESrevDispInitImpl();
    
    if (!handleGetFlag(801)) return;

    uint32_t MESrevLineBufUse = get_u32(MESrevLineBufUsePtr);
    uint32_t *MESrevDispLinePos = (uint32_t*)(void*)MESrevDispLinePosPtr;
    uint32_t *MESrevLineBufp = (uint32_t*)(void*)MESrevLineBufpPtr;
    unsigned short *MESrevText = (unsigned short*)(void*)MESrevTextPtr;
    uint32_t *MESrevDispLinePosY = (uint32_t*)(void*)MESrevDispLinePosYPtr;
    uint32_t *MESrevLineBufSize = (uint32_t*)(void*)MESrevLineBufSizePtr;

    for (int i = 0; i < MESrevLineBufUse; i++) {
        if ((short)MESrevText[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            overwrite_u32(MESrevDispMaxPtr, get_u32(MESrevDispMaxPtr) + 30);
            overwrite_u32(MESrevDispPosPtr, get_u32(MESrevDispPosPtr) + 30);
            for (int j = i; j < MESrevLineBufUse; j++) MESrevDispLinePosY[j] += 30;
        }
    }
}

void handleMESrevDispText(int fontSurfaceId, int maskSurfaceId, int param3, int param4,
                          int param5, int param6, int param7) {

    if (!handleGetFlag(801)) {
        MESrevDispTextImpl(fontSurfaceId, maskSurfaceId, param3, param4, param5, param6, param7);
        return;
    }

    uint32_t MESrevLineBufUse = get_u32(MESrevLineBufUsePtr);
    uint32_t *MESrevDispLinePos = (uint32_t*)(void*)MESrevDispLinePosPtr;
    uint32_t *MESrevLineBufp = (uint32_t*)(void*)MESrevLineBufpPtr;
    unsigned short *MESrevText = (unsigned short*)(void*)MESrevTextPtr;
    uint32_t *MESrevDispLinePosY = (uint32_t*)(void*)MESrevDispLinePosYPtr;
    uint32_t *MESrevLineBufSize = (uint32_t*)(void*)MESrevLineBufSizePtr;

    uint8_t *MESrevTextSize = (uint8_t*)(void*)MESrevTextSizePtr;
    uint32_t *MESrevTextPos = (uint32_t*)(void*)MESrevTextPosPtr;
    uint32_t MESrevDispPos = get_u32(MESrevDispPosPtr);
    uint8_t *MesFontColor = (uint8_t*)(void*)MesFontColorPtr;
    uint8_t *MESrevTextCol = (uint8_t*)(void*)MESrevTextColPtr;

    for (int i = 0; i < MESrevLineBufUse; i++) {
        if ((short)MESrevText[MESrevLineBufp[MESrevDispLinePos[i]]] < 0) {
            uint32_t iVar5 = (MESrevDispLinePosY[i] + param4) - MESrevDispPos - 17;
            uint32_t widthAccum = 150;
            for (uint nametagIndex = MESrevLineBufp[MESrevDispLinePos[i]] + 1;
                (short)MESrevText[nametagIndex] > 0;
                nametagIndex++) {
            
                uint32_t iVar15 = iVar5 + *(short*)((long)MESrevTextPos + (ulong)(nametagIndex << 1 | 1) * 2);
                uint32_t currWidth = (28 * ourTable[MESrevText[nametagIndex]] / 32) * 1.5f;

                handleGSLfontStretchWithMaskF(
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

void handleSpeakerDrawingFunction(float param1, float param2, float param3, float param4, float param5,
                                  float param6, int param7,   int param8,   uint param9,  int param10) {

    if (handleGetFlag(801) && param1 == 28.0f && param3 == 42.0f && param4 == 36.0f && param5 == 93.0f)
        param6 -= 45.0f;

    SpeakerDrawingFunctionImpl(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
}

uintptr_t OPTmenuModePtr;
uintptr_t OPTmenuCurPtr;
uintptr_t OPTmenuPagePtr;

enum ToggleSel {
    OFF,
    ON,
    INVALID
};

ToggleSel NPToggleSel = INVALID;

void handleOptionDispChip2(uint param_1) {
    OptionDispChip2Impl(param_1);

    uint32_t OPTmenuMode = *(uint32_t*)(void*)OPTmenuModePtr;
    uint8_t *OPTmenuCur = (uint8_t*)(void*)OPTmenuCurPtr;
    uint32_t OPTmenuPage = *(uint32_t*)(void*)OPTmenuPagePtr;

    // Nametag option text
    handleGSLflatRectF(152, (OPTmenuMode == 2 && OPTmenuCur[OPTmenuPage * 4] == 3) * 577.0f, 2959.0f, 147.0f, 35.0f, 242.0f, 602.0f, 0xFFFFFF, param_1, 1);
    // Divider
    handleGSLflatRectF(152, 0.0f, 1346.0f, 1443.0f, 6.0f, 238.0f, 643.0f, 0xFFFFFF, param_1, 1);

    // On/Off checkboxes
    handleGSLflatRectF(152, 1449.0f, 1086.0f, 115.0f, 40.0f, 1411.0f, 601.0f, 0xFFFFFF, param_1, 1);
    handleGSLflatRectF(152, 1449.0f, 1126.0f, 115.0f, 40.0f, 1537.0f, 601.0f, 0xFFFFFF, param_1, 1);

    if (OPTmenuMode == 2 && OPTmenuCur[OPTmenuPage * 4] == 3) {
        // Hover marker while selecting
        handleGSLflatRectF(152, 1517.0f, 1408.0f, 42.0f, 42.0f, 1414.0f + 126.0f * (int)(NPToggleSel == OFF), 596.0f, 0xFFFFFF, param_1, 1);
    }

    // Checkmark on currently toggled option
    handleGSLflatRectF(152, 1565.0f, 1408.0f, 42.0f, 42.0f, 1413.0f + 126.0f * (int)(!handleGetFlag(801)), 596.0f, 0xFFFFFF, param_1, 1);
}

void handleSSEvolume(uint param_1) {
    SSEvolumeImpl(param_1);
}

void handleSSEplay(int param_1, int param_2 = 0xFFFFFFFF) {
    SSEplayImpl(param_1, param_2);
}

uintptr_t PADcustomPtr;
uintptr_t PADrefPtr;
uintptr_t PADonePtr;
uintptr_t SYSSEvolPtr;

void handleOptionMain(void) {
    uint32_t *OPTmenuMode = (uint32_t*)(void*)OPTmenuModePtr;
    uint8_t *OPTmenuCur = (uint8_t*)(void*)OPTmenuCurPtr;
    uint32_t *OPTmenuPage = (uint32_t*)(void*)OPTmenuPagePtr;
    
    if (*OPTmenuMode != 2 || OPTmenuCur[*OPTmenuPage * 4] != 3) {
        OptionMainImpl();
        
        if (*OPTmenuMode == 2 && OPTmenuCur[*OPTmenuPage * 4] == 3) NPToggleSel = (ToggleSel)handleGetFlag(801);
        return;
    }
    
    // Nametag setting is selected
    
    uint32_t *PADcustom = (uint32_t*)(void*)PADcustomPtr;
    uint32_t PADref = *(uint32_t*)(void*)PADrefPtr;
    uint32_t PADone = *(uint32_t*)(void*)PADonePtr;

    // PADcustom[2] = D-Pad Left
    // PADcustom[3] = D-Pad Right

    // PADcustom[5] = A
    // PADcustom[6] = B

    uint32_t SYSSEvol = *(uint32_t*)(void*)SYSSEvolPtr;

    if (((PADcustom[2] | PADcustom[3]) & PADref) != 0) {
        handleSSEvolume(SYSSEvol);
        handleSSEplay(1);
        NPToggleSel = ToggleSel(NPToggleSel ^ 1);
    } else if ((PADcustom[5] & PADone) != 0) {
        handleSSEvolume(SYSSEvol);
        handleSSEplay(2);
        handleSetFlag(801, NPToggleSel);
        overwrite_u32(OPTmenuModePtr, 1); 
    } else if ((PADcustom[6] & PADone) != 0) {
        handleSSEvolume(SYSSEvol);
        handleSSEplay(3);
        overwrite_u32(OPTmenuModePtr, 1); 
    }
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

void hookFunctions() {
    MEStextDatNumPtr = retrievePointer(sigScan("game", "MEStextDatNumPtr"), -0xC);
    MESngFontListTopNumPtr = retrievePointer(sigScan("game", "MESngFontListTopNumPtr"), 0x8);
    MESngFontListLastNumPtr = retrievePointer(sigScan("game", "MESngFontListLastNumPtr"), 0x8);
    MEStextFlPtr = retrievePointer(sigScan("game", "MEStextFlPtr"), 0xC);
    MEStextPtr = retrievePointer(sigScan("game", "MEStextPtr"), 0x10);
    MESngFontListLastPtr = retrievePointer(sigScan("game", "MESngFontListLastPtr"), 0x20);
    MESngFontListTopPtr = retrievePointer(sigScan("game", "MESngFontListTopPtr"), 0x24);
    ScrWorkPtr = retrievePointer(sigScan("game", "ScrWorkPtr"));
    MesFontColorPtr = retrievePointer(sigScan("game", "MesFontColorPtr"), 0x18);
    EPmaxPtr = retrievePointer(sigScan("game", "EPmaxPtr"));

    uintptr_t MESsetNGflagAddr = sigScan("game", "MESsetNGflag");

    MEStextDatNumPtr = retrievePointer(MESsetNGflagAddr + 28, -0xC);
    MESngFontListTopNumPtr = retrievePointer(MESsetNGflagAddr + 28, 0x8);
    MESngFontListLastNumPtr = retrievePointer(MESsetNGflagAddr + 32, 0x8);
    MEStextFlPtr = retrievePointer(MESsetNGflagAddr + 48, 0xC);
    MEStextPtr = retrievePointer(MESsetNGflagAddr + 52, 0x10);
    MESngFontListLastPtr = retrievePointer (MESsetNGflagAddr + 56, 0x20);
    MESngFontListTopPtr = retrievePointer(MESsetNGflagAddr + 64, 0x24);

    uintptr_t MESrevDispTextAddr = sigScan("game", "MESrevDispText");
    uintptr_t SystemMenuDispAddress = sigScan("game", "SystemMenuDisp");

    MESrevLineBufUsePtr = retrievePointer(MESrevDispTextAddr + 40, 20);
    MESrevDispMaxPtr = retrievePointer(SystemMenuDispAddress + 0x778);
    MESrevTextSizePtr = MESrevTextPtr + 0x493e0;
    MESrevTextPosPtr = MESrevTextPtr + 0x186a0;
    MESrevDispPosPtr = retrievePointer(MESrevDispTextAddr + 0x124);
    MESrevTextColPtr = MESrevTextSizePtr + 0x30d40;

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "GSLfontStretchF")),
        reinterpret_cast<void*>(handleGSLfontStretchF),
        (void **)&GSLfontStretchFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "GSLfontStretchWithMaskF")),
        reinterpret_cast<void*>(handleGSLfontStretchWithMaskF),
        (void **)&GSLfontStretchWithMaskFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "GSLfontStretchWithMaskExF")),
        reinterpret_cast<void*>(handleGSLfontStretchWithMaskExF),
        (void **)&GSLfontStretchWithMaskExFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "TipsDataInit")),
        reinterpret_cast<void*>(handleTipsDataInit),
        (void **)&TipsDataInitImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(MESsetNGflagAddr),
        reinterpret_cast<void*>(handleMESsetNGflag),
        (void **)&MESsetNGflagImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "ChatLayout")),
        reinterpret_cast<void*>(handleChatLayout),
        (void **)&ChatLayoutImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "calMain")),
        reinterpret_cast<void*>(handlecalMain),
        (void **)&calMainImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "ChatRendering")),
        reinterpret_cast<void*>(handleChatRendering),
        (void **)&ChatRenderingImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "MESdrawTextExF")),
        reinterpret_cast<void*>(handleMESdrawTextExF),
        (void **)&MESdrawTextExFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "GSLflatRectF")),
        reinterpret_cast<void*>(handleGSLflatRectF),
        (void **)&GSLflatRectFImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "SetFlag")),
        reinterpret_cast<void*>(handleSetFlag),
        (void **)&SetFlagImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "GetFlag")),
        reinterpret_cast<void*>(handleGetFlag),
        (void **)&GetFlagImpl
    );
    
    A64HookFunction(
        reinterpret_cast<void*>(MESrevDispTextAddr),
        reinterpret_cast<void*>(handleMESrevDispText),
        (void **)&MESrevDispTextImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "MESrevDispInit")),
        reinterpret_cast<void*>(handleMESrevDispInit),
        (void **)&MESrevDispInitImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "SpeakerDrawingFunction")),
        reinterpret_cast<void*>(handleSpeakerDrawingFunction),
        (void **)&SpeakerDrawingFunctionImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "OptionDispChip2")),
        reinterpret_cast<void*>(handleOptionDispChip2),
        (void **)&OptionDispChip2Impl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "OptionMain")),
        reinterpret_cast<void*>(handleOptionMain),
        (void **)&OptionMainImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "SSEvolume")),
        reinterpret_cast<void*>(handleSSEvolume),
        (void **)&SSEvolumeImpl
    );

    A64HookFunction(
        reinterpret_cast<void*>(sigScan("game", "SSEplay")),
        reinterpret_cast<void*>(handleSSEplay),
        (void **)&SSEplayImpl
    );
}

static skyline::utils::Task* after_romfs_task = new skyline::utils::Task{[]() {
    loadWidths();
    configInit();
    hookFunctions();
}};

void stub() {}

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
    const char *fontAlinePattern =                          "20111111";
    const char *audioLoweringPattern =                      "187F0153";
    const std::vector<const char *> widthCheckPatterns =    {"1F790571", "3F790571", "3F7D0571", "5F790571", "7F790571", "7F3D0671", "BF3D0671", "7F7D0571", "5F3D0671"};
    const char *SaveMenuGuidePattern =                      "010000001027";

    codeCaves = skyline::utils::g_MainRodataAddr - 0xC30;
    englishTipsHideBranch1 = code + 0x248a8;
    englishTipsShowBranch1 = code + 0x248c0;
    englishTipsHideBranch2 = code + 0x2499c;
    englishTipsShowBranch2 = code + 0x249b4;

    MESrevTextPtr = code + 0x7b0780;
    MESrevLineBufpPtr = code + 0x842f4c;
    MESrevDispLinePosPtr = code + 0xa4a4d0;
    MESrevDispLinePosYPtr = code + 0xa4b794;
    MESrevLineBufSizePtr = code + 0x873c8c;
    fontDrawModePtr = code + 0x16a3e8;
    OPTmenuModePtr = code + 0x78cbcc;
    OPTmenuCurPtr = code + 0x78cbd4;
    OPTmenuPagePtr = code + 0x78cbd0;
    PADcustomPtr = code + 0xa4e690;
    PADrefPtr = code + 0x77eebc;
    PADonePtr = code + 0x77ee94;
    SYSSEvolPtr = code + 0x1a07e98;
    MesNameDispLenPtr = code + 0xa49674;

    uintptr_t OPTmenuCurMaxPtr = code + 0x16b194;
    uint8_t *OPTmenuCurMax = (uint8_t*)(void*)OPTmenuCurMaxPtr;
    OPTmenuCurMax[4] = 4;

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

    // Skip fix
    overwrite_u32(code + 0x437a8, 0x17FFFFB9);

    // DoZ selection
    overwrite_u32(code + 0x2bc88, 0x17FFFF39);
    overwrite_u32(code + 0x2baac, 0x17FFFFB0);

    // Fix shortcut bug
    overwrite_u32(code + 0x2c140, 0x52806E00);

}   

extern "C" void skyline_init() {
    skyline::utils::init();
    virtmemSetup();  // needed for libnx JIT

    skyline_main();
}
