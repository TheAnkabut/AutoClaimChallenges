// ==================== PopupDismiss - Block challenge popups via function flag manipulation ====================
//
// Blocks TWO functions during auto-claim + 30s:
//   1. HandleChallengeRewardCollected - challenge reward popup
//   2. HandleNewOnlineItem - product drop popups (items/XP from server push)
//
// Both must be blocked to prevent ALL popups. Items are still saved to inventory
// (SaveData.GiveChallengeDrop runs before these functions).
//

#include "pch.h"
#include "PopupDismiss.h"
#include "Internals.h"
#include "../logging.h"

namespace PopupDismiss
{
    // ==================== State ====================

    static constexpr size_t FLAGS_OFFSET = 0x130;

    struct BlockedFunc
    {
        UFunction* func = nullptr;
        uint64_t originalFlags = 0;
        const char* name = nullptr;
    };

    static BlockedFunc blockedFuncs[2];
    static bool blocked = false;

    // ==================== Core ====================

    void BlockPopups()
    {
        if (blocked) return;

        for (auto& bf : blockedFuncs)
        {
            if (!bf.func) continue;

            auto* flagsPtr = reinterpret_cast<uint64_t*>(
                reinterpret_cast<uintptr_t>(bf.func) + FLAGS_OFFSET);

            bf.originalFlags = *flagsPtr;
            *flagsPtr = 0;
        }

        blocked = true;
        LOG("[DISMISS] Blocked {} functions (flags zeroed)", 2);
    }

    void RestorePopups()
    {
        if (!blocked) return;

        for (auto& bf : blockedFuncs)
        {
            if (!bf.func) continue;

            auto* flagsPtr = reinterpret_cast<uint64_t*>(
                reinterpret_cast<uintptr_t>(bf.func) + FLAGS_OFFSET);

            *flagsPtr = bf.originalFlags;
        }

        blocked = false;
        LOG("[DISMISS] Restored {} functions (flags restored)", 2);
    }

    // ==================== Lifecycle ====================

    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        blockedFuncs[0] = {
            UFunction::FindFunction("Function TAGame.GFxData_MultiItemDrops_TA.HandleChallengeRewardCollected"),
            0,
            "HandleChallengeRewardCollected"
        };

        blockedFuncs[1] = {
            UFunction::FindFunction("Function TAGame.GFxData_MultiItemDrops_TA.HandleNewOnlineItem"),
            0,
            "HandleNewOnlineItem"
        };

        for (const auto& bf : blockedFuncs)
        {
            LOG("[DISMISS] {}: {}", bf.name, bf.func ? "OK" : "FAILED");
        }
    }
}
