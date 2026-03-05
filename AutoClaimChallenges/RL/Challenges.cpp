// ==================== Challenges - Challenge mapping, claiming, and stats ====================

#include "pch.h"
#include "Challenges.h"
#include "Internals.h"
#include "PopupDismiss.h"
#include <unordered_set>
#include <vector>
#include <ctime>

namespace Challenges
{
    // ==================== Memory Offsets ====================

    constexpr int OFFSET_CHALLENGE_ID       = 0x60;
    constexpr int OFFSET_CHALLENGE_FLAGS    = 0x1F0;
    constexpr int OFFSET_FOLDER_IDS        = 0xA0;
    constexpr int OFFSET_FOLDER_START_TIME  = 0x88;
    constexpr int OFFSET_FOLDER_END_TIME    = 0x90;
    constexpr int OFFSET_MANAGER_CHALLENGES = 0x60;

    // ==================== Types (internal only) ====================

    struct FUniqueNetId
    {
        uint8_t Uid[8];
        uint8_t NpId[0x28];
        uint8_t EpicAccountId[0x10];
        uint8_t Platform;
        uint8_t SplitscreenId;
        uint8_t Padding[6];
    };
    static_assert(sizeof(FUniqueNetId) == 0x48, "FUniqueNetId size mismatch");

    enum class EChallengeType
    {
        WEEKLY,
        SEASON,
        EVENT
    };

    struct TypeStats
    {
        int total     = 0;
        int claimable = 0;
        int claimed   = 0;
    };

    // ==================== State ====================

    static std::shared_ptr<GameWrapper> gameWrapper;
    static std::unordered_map<int32_t, EChallengeType> mapCache;
    static std::unordered_map<int, uint64_t> expiryEndTimes;
    static bool leftMatch = false;

    static const std::unordered_set<int32_t> PERMANENT_UNKNOWNS = {
        584, 585, 586, 587, 589, 590, 591
    };

    // Objects resolved once in Initialize
    static UObject* psyNet = nullptr;
    static UFunction* rpcCreateFunc = nullptr;
    static UFunction* rpcQueueFunc = nullptr;
    static UClass* rewardRpcClass = nullptr;
    static UFunction* setPlayerIdFunc = nullptr;
    static UFunction* setChallengeIdFunc = nullptr;
    static UObject* challengeManager = nullptr;

    // Stats (refreshed per Render frame)
    static std::unordered_map<std::string, TypeStats> cachedStats;
    static bool cachedManagerReady = false;
    static bool cachedFinalRewardAvailable = false;

    // Sequential claim queue
    static std::vector<int32_t> claimQueue;
    static size_t claimIndex = 0;
    static FUniqueNetId cachedPlayerId;
    static bool claimingInProgress = false;

    // ==================== Helpers ====================

    static FUniqueNetId GetPlayerId()
    {
        FUniqueNetId playerId;
        memset(&playerId, 0, sizeof(playerId));

        auto uniqueIdWrapper = gameWrapper->GetUniqueID();
        memcpy(&playerId, &uniqueIdWrapper, (std::min)(sizeof(playerId), sizeof(uniqueIdWrapper)));

        return playerId;
    }

    // ==================== RPC claiming ====================

    static void ExecuteClaimRPC(int32_t challengeId, const FUniqueNetId& playerId)
    {
        struct
        {
            UClass* RPCClass;
            UObject* ReturnValue;
        } createParams = { rewardRpcClass, nullptr };

        psyNet->ProcessEvent(rpcCreateFunc, &createParams);
        UObject* rpcInstance = createParams.ReturnValue;

        struct
        {
            FUniqueNetId InPlayerID;
            UObject* ReturnValue;
        } playerParams = {};

        playerParams.InPlayerID = playerId;
        rpcInstance->ProcessEvent(setPlayerIdFunc, &playerParams);

        struct
        {
            int32_t ChallengeID;
            UObject* ReturnValue;
        } challengeParams = {};

        challengeParams.ChallengeID = challengeId;
        rpcInstance->ProcessEvent(setChallengeIdFunc, &challengeParams);

        struct
        {
            UObject* RPC;
            UObject* ReturnValue;
        } queueParams = { rpcInstance, nullptr };

        psyNet->ProcessEvent(rpcQueueFunc, &queueParams);
    }

    static void ClaimNext()
    {
        if (claimIndex >= claimQueue.size())
        {
            return;
        }

        LOG("[CLAIM] Sending RPC {}/{} (ID: {})...", claimIndex + 1, claimQueue.size(), claimQueue[claimIndex]);
        ExecuteClaimRPC(claimQueue[claimIndex], cachedPlayerId);
        claimIndex++;

        // Schedule next claim (1s apart to avoid batching issues)
        if (claimIndex < claimQueue.size())
        {
            gameWrapper->SetTimeout(
                [](GameWrapper*)
                {
                    if (claimingInProgress)
                    {
                        ClaimNext();
                    }
                },
                1.0f);
        }
        else
        {
            LOG("[CLAIM] All {}/{} claims sent.", claimQueue.size(), claimQueue.size());
            claimingInProgress = false;

            LOG("[DISMISS] Keeping block for 30s to catch late push notifications...");
            gameWrapper->SetTimeout(
                [](GameWrapper*)
                {
                    PopupDismiss::RestorePopups();
                },
                30.0f);
        }
    }

    // ==================== Map & Stats ====================

    static void BuildMap()
    {
        mapCache.clear();
        expiryEndTimes.clear();

        uint64_t now = static_cast<uint64_t>(time(nullptr));
        auto* allObjects = UObject::GObjObjects();

        for (int i = 0; i < allObjects->Count; ++i)
        {
            UObject* object = (*allObjects)[i];
            if (!object) continue;
            if (object->GetFullName() != "ChallengeFolder_TA Transient.ChallengeFolder_TA")
            {
                continue;
            }

            // CodeName (0x60) is always English 
            wchar_t** codeNamePtr = reinterpret_cast<wchar_t**>((uint8_t*)object + 0x60);
            int32_t codeNameLen = *reinterpret_cast<int32_t*>((uint8_t*)object + 0x68);

            if (!*codeNamePtr || codeNameLen <= 0)
            {
                continue;
            }

            std::wstring wide(*codeNamePtr, codeNameLen);
            std::string codeName(wide.begin(), wide.end());

            // Classify by CodeName: Week10, Season21.Weekly, Season21.Season, Event.Goalentines2026, etc.
            EChallengeType folderType;
            if (codeName.find("Week") != std::string::npos)
            {
                folderType = EChallengeType::WEEKLY;
            }
            else if (codeName.find("Season") != std::string::npos)
            {
                folderType = EChallengeType::SEASON;
            }
            else
            {
                folderType = EChallengeType::EVENT;
            }

            uint64_t endTime = *reinterpret_cast<uint64_t*>((uint8_t*)object + OFFSET_FOLDER_END_TIME);
            uint64_t startTime = *reinterpret_cast<uint64_t*>((uint8_t*)object + OFFSET_FOLDER_START_TIME);

            if (endTime > now && startTime <= now)
            {
                int typeKey = static_cast<int>(folderType);
                auto existing = expiryEndTimes.find(typeKey);
                if (existing == expiryEndTimes.end() || endTime < existing->second)
                {
                    expiryEndTimes[typeKey] = endTime;
                }
            }

            TArray<int32_t>* challengeIds = reinterpret_cast<TArray<int32_t>*>((uint8_t*)object + OFFSET_FOLDER_IDS);

            for (int j = 0; j < challengeIds->Count; ++j)
            {
                int32_t challengeId = (*challengeIds)[j];
                if (folderType == EChallengeType::SEASON || mapCache.find(challengeId) == mapCache.end())
                {
                    mapCache[challengeId] = folderType;
                }
            }
        }
    }

    static void RefreshStats()
    {
        cachedStats.clear();
        cachedManagerReady = false;
        cachedFinalRewardAvailable = false;

        if (!challengeManager)
        {
            challengeManager = Internals::FindObject("ChallengeManager_TA Transient.GameEngine_TA.LocalPlayer_TA.OnlinePlayer_TA.ChallengeManager_TA");
        }
        if (!challengeManager)
        {
            return;
        }

        auto* challengeArray = reinterpret_cast<TArray<UObject*>*>((uint8_t*)challengeManager + OFFSET_MANAGER_CHALLENGES);
        cachedManagerReady = true;

        for (int i = 0; i < challengeArray->Count; ++i)
        {
            UObject* challenge = (*challengeArray)[i];

            int32_t challengeId = *reinterpret_cast<int32_t*>((uint8_t*)challenge + OFFSET_CHALLENGE_ID);
            uint32_t challengeFlags = *reinterpret_cast<uint32_t*>((uint8_t*)challenge + OFFSET_CHALLENGE_FLAGS);

            bool isCompleted = (challengeFlags & 0x08) != 0;
            bool hasRewards  = (challengeFlags & 0x02) != 0;

            auto mapEntry = mapCache.find(challengeId);
            const char* typeName = "UNKNOWN";
            if (mapEntry != mapCache.end())
            {
                switch (mapEntry->second)
                {
                case EChallengeType::WEEKLY: typeName = "WEEKLY"; break;
                case EChallengeType::SEASON: typeName = "SEASON"; break;
                case EChallengeType::EVENT:  typeName = "EVENT";  break;
                }
            }

            cachedStats[typeName].total++;

            if (isCompleted && hasRewards)
            {
                cachedStats[typeName].claimable++;
            }
            else if (isCompleted && !hasRewards)
            {
                cachedStats[typeName].claimed++;
            }

            if (isCompleted && hasRewards && mapEntry == mapCache.end() && PERMANENT_UNKNOWNS.find(challengeId) == PERMANENT_UNKNOWNS.end())
            {
                cachedFinalRewardAvailable = true;
            }
        }
    }

    // ==================== Lifecycle ====================

    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        Challenges::gameWrapper = wrapper;

        // Register CVars owned by this module
        _globalCvarManager->registerCvar("acc_auto_claim", "1", "Auto-claim after match", true, false, 0, false, 0, true);
        _globalCvarManager->registerCvar("acc_claim_before_expiry", "0", "Auto-claim before expiry", true, false, 0, false, 0, true);
        _globalCvarManager->registerCvar("acc_days_before_expiry", "2", "Days before expiry to claim", true, true, 1, true, 7, true);

        psyNet = Internals::FindObject("PsyNet_X Transient.PsyNet_X");
        rpcCreateFunc = UFunction::FindFunction("Function ProjectX.PsyNet_X.RPC");
        rpcQueueFunc = UFunction::FindFunction("Function ProjectX.PsyNet_X.QueueRPC");
        rewardRpcClass = static_cast<UClass*>(Internals::FindObject("Class TAGame.RPC_Challenge_RequestReward_TA"));
        setPlayerIdFunc = UFunction::FindFunction("Function TAGame.RPC_Challenge_RequestReward_TA.SetPlayerID");
        setChallengeIdFunc = UFunction::FindFunction("Function TAGame.RPC_Challenge_RequestReward_TA.SetChallengeID");
        challengeManager = Internals::FindObject("ChallengeManager_TA Transient.GameEngine_TA.LocalPlayer_TA.OnlinePlayer_TA.ChallengeManager_TA");

        LOG("[INIT] PsyNet: {}", psyNet ? "OK" : "FAILED");
        LOG("[INIT] ChallengeManager: {}", challengeManager ? "OK" : "FAILED");

        BuildMap();

        gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnMatchEnded",
            [](std::string)
            {
                leftMatch = true;
            });

        gameWrapper->HookEvent("Function TAGame.GFxData_MainMenu_TA.MainMenuAdded",
            [](std::string)
            {
                BuildMap();

                if (leftMatch && _globalCvarManager->getCvar("acc_auto_claim").getBoolValue())
                {
                    leftMatch = false;
                    ClaimAll();
                    return; // ClaimAll already covers everything
                }
                leftMatch = false;

                if (_globalCvarManager->getCvar("acc_claim_before_expiry").getBoolValue())
                {
                    CheckExpiryAndClaim();
                }
            });

        gameWrapper->HookEvent("Function TAGame.ChallengeManager_TA.EventChallengeRewardCollected",
            [](std::string)
            {
                LOG("[REWARD] Server confirmed reward collected.");
            });

        gameWrapper->SetTimeout(
            [](GameWrapper*)
            {
                if (_globalCvarManager->getCvar("acc_claim_before_expiry").getBoolValue())
                {
                    BuildMap();
                    CheckExpiryAndClaim();
                }
            },
            5.0f);
    }

    void ClaimByType(const EChallengeType* filterType)
    {
        if (claimingInProgress)
        {
            LOG("[CLAIM] Already claiming - ignoring.");
            return;
        }

        if (!challengeManager)
        {
            return;
        }

        auto* challengeArray = reinterpret_cast<TArray<UObject*>*>((uint8_t*)challengeManager + OFFSET_MANAGER_CHALLENGES);

        claimQueue.clear();
        claimIndex = 0;

        for (int i = 0; i < challengeArray->Count; ++i)
        {
            UObject* challenge = (*challengeArray)[i];

            int32_t challengeId = *reinterpret_cast<int32_t*>((uint8_t*)challenge + OFFSET_CHALLENGE_ID);
            uint32_t challengeFlags = *reinterpret_cast<uint32_t*>((uint8_t*)challenge + OFFSET_CHALLENGE_FLAGS);

            bool isCompleted = (challengeFlags & 0x08) != 0;
            bool hasRewards  = (challengeFlags & 0x02) != 0;

            if (!isCompleted || !hasRewards || PERMANENT_UNKNOWNS.find(challengeId) != PERMANENT_UNKNOWNS.end())
            {
                continue;
            }

            if (filterType)
            {
                auto mapEntry = mapCache.find(challengeId);
                if (mapEntry == mapCache.end() || mapEntry->second != *filterType)
                {
                    continue;
                }
            }

            claimQueue.push_back(challengeId);
        }

        if (claimQueue.empty())
        {
            LOG("[CLAIM] Nothing to claim.");
            return;
        }

        cachedPlayerId = GetPlayerId();
        claimingInProgress = true;
        PopupDismiss::BlockPopups();
        LOG("[CLAIM] Starting claim of {} challenges...", claimQueue.size());

        ClaimNext();
    }

    void ClaimAll()
    {
        gameWrapper->Execute([](GameWrapper*) { ClaimByType(nullptr); });
    }

    void CheckExpiryAndClaim()
    {
        uint64_t now = static_cast<uint64_t>(time(nullptr));
        uint64_t margin = static_cast<uint64_t>(_globalCvarManager->getCvar("acc_days_before_expiry").getIntValue()) * 86400;

        for (auto& [typeKey, endTime] : expiryEndTimes)
        {
            if (now >= endTime - margin)
            {
                LOG("[EXPIRY] Type {} within expiry window - claiming all.", typeKey);
                ClaimAll();
                return;
            }
        }
    }

    bool IsManagerReady()
    {
        RefreshStats();
        return cachedManagerReady;
    }

    bool IsFinalRewardAvailable()
    {
        return cachedFinalRewardAvailable;
    }

    void GetTypeStats(const char* typeKey, int& total, int& claimable, int& claimed)
    {
        auto statsEntry = cachedStats.find(typeKey);
        if (statsEntry != cachedStats.end())
        {
            total     = statsEntry->second.total;
            claimable = statsEntry->second.claimable;
            claimed   = statsEntry->second.claimed;
        }
        else
        {
            total     = 0;
            claimable = 0;
            claimed   = 0;
        }
    }

    void ClaimByTypeKey(int typeKey)
    {
        gameWrapper->Execute([typeKey](GameWrapper*)
        {
            EChallengeType type = static_cast<EChallengeType>(typeKey);
            ClaimByType(&type);
        });
    }
}
