#include <string_view>
#include <optional>

#include "System.h"
#include "Mem.h"

namespace rd {
namespace sys {

enum class NametagOptionLayoutImpl {
    CHNENG,
    CHNJPN
};

std::optional<NametagOptionLayoutImpl> NametagOptionLayoutFromString(std::string_view from) {
    if (from == "chneng") return NametagOptionLayoutImpl::CHNENG;
    if (from == "chnjpn") return NametagOptionLayoutImpl::CHNJPN;
    return std::nullopt;
}

static auto NametagOptionLayout = std::optional<NametagOptionLayoutImpl>();

void GSLflatRectF::Callback(int textureId, float spriteX, float spriteY,
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
    Orig(textureId, spriteX, spriteY, spriteWidth,
                     spriteHeight, displayX, displayY, color,
                     opacity, unk);
}

void SetFlag::Callback(uint flag, uint setValue) {
    Orig(flag, setValue);
}

bool GetFlag::Callback(uint flag) {
    bool res = Orig(flag);
    if (flag != 3877) return res;
    return Orig(873) && res;
}

void SpeakerDrawingFunction::Callback(float param1, float param2, float param3, float param4, float param5,
                                  float param6, int param7,   int param8,   uint param9,  int param10) {

    if (GetFlag::Callback(801) && param1 == 28.0f && param3 == 42.0f && param4 == 36.0f && param5 == 93.0f)
        param6 -= 65.0f;

    Orig(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10);
}

enum class ToggleSel : uint8_t {
    OFF,
    ON,
};

static auto NPToggleSel = std::optional<ToggleSel>();

// OptionMain always gets called first, so we can safely dereference NametagOptionLayout
void OptionDispChip2::Callback(uint param_1) {
    Orig(param_1);

    if (!NametagOptionLayout) return;

    const bool selecting = *OPTmenuModePtr == 2 && OPTmenuCur[*OPTmenuPagePtr] == 3;

    switch (*NametagOptionLayout) {
        case NametagOptionLayoutImpl::CHNJPN: {
            // Nametag option text
            GSLflatRectF::Callback(152, 0.0f, 2326.0f + (int)(selecting) * 40.0f, 640.0f, 34.0f, 242.0f, 605.0f, 0xFFFFFF, param_1, 1);
            // Divider
            GSLflatRectF::Callback(152, 0.0f, 1346.0f, 1443.0f, 6.0f, 238.0f, 643.0f, 0xFFFFFF, param_1, 1);

            // On/Off checkboxes
            GSLflatRectF::Callback(152, 1449.0f, 1086.0f, 449.0f, 38.0f, 1221.0f, 601.0f, 0xFFFFFF, param_1, 1);

            if (selecting) {
                // Hover marker while selecting
                GSLflatRectF::Callback(152, 1517.0f, 1396.0f, 42.0f, 42.0f, 1428.0f + 127.0f * (int)(*NPToggleSel == ToggleSel::OFF), 595.0f, 0xFFFFFF, param_1, 1);
            }

            // Checkmark on currently toggled option
            GSLflatRectF::Callback(152, 1565.0f, 1396.0f, 42.0f, 42.0f, 1428.0f + 127.0f * (int)(!GetFlag::Callback(801)), 595.0f, 0xFFFFFF, param_1, 1);
            
            break;
        } 
        case NametagOptionLayoutImpl::CHNENG: {
        // Nametag option text
            GSLflatRectF::Callback(152, (int)selecting * 577.0f, 2959.0f, 147.0f, 35.0f, 242.0f, 602.0f, 0xFFFFFF, param_1, 1);
            // Divider
            GSLflatRectF::Callback(152, 0.0f, 1346.0f, 1443.0f, 6.0f, 238.0f, 643.0f, 0xFFFFFF, param_1, 1);

            // On/Off checkboxes
            GSLflatRectF::Callback(152, 1449.0f, 1086.0f, 115.0f, 40.0f, 1411.0f, 601.0f, 0xFFFFFF, param_1, 1);
            GSLflatRectF::Callback(152, 1449.0f, 1126.0f, 115.0f, 40.0f, 1537.0f, 601.0f, 0xFFFFFF, param_1, 1);
            
            if (selecting) {
                // Hover marker while selecting
                GSLflatRectF::Callback(152, 1517.0f, 1408.0f, 42.0f, 42.0f, 1414.0f + 126.0f * (int)(*NPToggleSel == ToggleSel::OFF), 596.0f, 0xFFFFFF, param_1, 1);
            }

            // Checkmark on currently toggled option
            GSLflatRectF::Callback(152, 1565.0f, 1408.0f, 42.0f, 42.0f, 1413.0f + 126.0f * (int)(!GetFlag::Callback(801)), 596.0f, 0xFFFFFF, param_1, 1);
        
            break;
        }
        default:
            UNREACHABLE;
    }
}

void SSEvolume::Callback(uint param_1) {
    Orig(param_1);
}

void SSEplay::Callback(int param_1, int param_2 = 0xFFFFFFFF) {
    Orig(param_1, param_2);
}

void OptionMain::Callback(void) {
    if (*OPTmenuModePtr != 2 || *OPTmenuPagePtr != 1 || OPTmenuCur[*OPTmenuPagePtr] != 3) {
        Orig();
        
        if (*OPTmenuModePtr == 2 && *OPTmenuPagePtr == 1 && OPTmenuCur[*OPTmenuPagePtr] == 3) NPToggleSel = (ToggleSel)GetFlag::Callback(801);
        return;
    }
    
    // Nametag setting is selected

    // PADcustom[2] = D-Pad Left
    // PADcustom[3] = D-Pad Right

    // PADcustom[5] = A
    // PADcustom[6] = B

    if (((PADcustom[2] | PADcustom[3]) & *PADrefPtr) != 0) {
        SSEvolume::Callback(*SYSSEvolPtr);
        SSEplay::Callback(1);
        NPToggleSel = ToggleSel(static_cast<uint8_t>(*NPToggleSel) ^ 1);
    } else if ((PADcustom[5] & *PADonePtr) != 0) {
        SSEvolume::Callback(*SYSSEvolPtr);
        SSEplay::Callback(2);
        SetFlag::Callback(801, static_cast<uint8_t>(*NPToggleSel));
        *OPTmenuModePtr = 1; 
    } else if ((PADcustom[6] & *PADonePtr) != 0) {
        SSEvolume::Callback(*SYSSEvolPtr);
        SSEplay::Callback(3);
        *OPTmenuModePtr = 1; 
    }
}

void OptionDefault::Callback(void) {
    // Not in text settings, nothing to do
    // (Resetting options to default only applies to the current page)
    if (*OPTmenuPagePtr != 1) return;

    NPToggleSel = ToggleSel::OFF;
    SetFlag::Callback(801, false);
}

bool ChkViewDic::Callback(uint param_1, uint param_2) {
    return Orig(param_1, param_2);
}

void Init() {
    HOOK_VAR(game, ScrWork);
    HOOK_VAR(game, OPTmenuModePtr);
    HOOK_VAR(game, OPTmenuCur);
    HOOK_VAR(game, OPTmenuPagePtr);
    HOOK_VAR(game, PADcustom);
    HOOK_VAR(game, PADrefPtr);
    HOOK_VAR(game, PADonePtr);
    HOOK_VAR(game, SYSSEvolPtr);

    if (rd::config::config["gamedef"]["signatures"]["game"].has("SaveMenuGuide")) {
        uint32_t *SaveMenuGuide = (uint32_t*)rd::hook::SigScan("game", "SaveMenuGuide");
        uint32_t buttonIds[] = { 0, 10010, 2, 10020, 6, 7, 10100, 1, 10000, 255 };
        ::memcpy(&SaveMenuGuide[6 * 20], buttonIds, sizeof(buttonIds));
    }

    if (rd::config::config["gamedef"]["signatures"]["game"].has("audioLoweringAddr")) {
        uintptr_t audioLoweringAddr = rd::hook::SigScan("game", "audioLoweringAddr");
        uint32_t nop = inst::Nop().Value();

        rd::mem::Overwrite(audioLoweringAddr,     nop);
        rd::mem::Overwrite(audioLoweringAddr + 4, nop);
    }

    
    if (rd::config::config["gamedef"]["signatures"]["game"].has("SkipModeFix"))
        rd::mem::Overwrite(rd::hook::SigScan("game", "SkipModeFix"), inst::Branch(-284).Value());

    if (rd::config::config["gamedef"]["signatures"]["game"].has("DoZSelection1"))
        rd::mem::Overwrite(rd::hook::SigScan("game", "DoZSelection1"), inst::Branch(-796).Value());

    if (rd::config::config["gamedef"]["signatures"]["game"].has("DoZSelection2"))
        rd::mem::Overwrite(rd::hook::SigScan("game", "DoZSelection2"), inst::Branch(-320).Value());
    
    if (rd::config::config["gamedef"]["signatures"]["game"].has("ShortcutMenuFix"))
        rd::mem::Overwrite(rd::hook::SigScan("game", "ShortcutMenuFix"), inst::Movz(reg::W0, 0x370).Value());

    HOOK_FUNC(game, GSLflatRectF);
    HOOK_FUNC(game, SetFlag);
    HOOK_FUNC(game, GetFlag);

    if (rd::config::config["patchdef"]["base"]["addNametags"].get<bool>()) {
     
        if (rd::config::config["gamedef"]["signatures"]["game"].has("OPTmenuMaxCur")) {
            uint8_t *OPTmenuMaxCur = (uint8_t*)rd::hook::SigScan("game", "OPTmenuMaxCur");
            OPTmenuMaxCur[4] = 4;
        }

        NametagOptionLayout = NametagOptionLayoutFromString(
            rd::config::config["patchdef"]["base"]["nametagOptionLayout"].get<std::string_view>()
        );

        HOOK_FUNC(game, SpeakerDrawingFunction);
        HOOK_FUNC(game, OptionDispChip2);
        HOOK_FUNC(game, OptionMain);
        HOOK_FUNC(game, OptionDefault);
    }

    HOOK_FUNC(game, SSEvolume);
    HOOK_FUNC(game, SSEplay);
    HOOK_FUNC(game, ChkViewDic);
}

}  // namespace sys
}  // namespace rd
