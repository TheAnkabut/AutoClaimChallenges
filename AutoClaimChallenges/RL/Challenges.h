#pragma once

namespace Challenges
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void ClaimAll();
    void CheckExpiryAndClaim();
    void ClaimByTypeKey(int typeKey);

    bool IsManagerReady();
    bool IsFinalRewardAvailable();
    void GetTypeStats(const char* typeKey, int& total, int& claimable, int& claimed);
}
