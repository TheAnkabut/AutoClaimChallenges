#include "pch.h"
#include "Settings.h"
#include "../RL/Challenges.h"
#include "../Utils/Localization.h"

namespace Settings
{
    // ==================== Helpers ====================

    static void RenderTypeRow(
        const char* titleKey,
        const char* typeKey,
        ImVec4 color)
    {
        int total     = 0;
        int claimable = 0;
        int claimed   = 0;

        Challenges::GetTypeStats(typeKey, total, claimable, claimed);

        ImGui::TextColored(color, "%s", L(titleKey));
        ImGui::NextColumn();
        ImGui::Text("%d", claimable);
        ImGui::NextColumn();
        ImGui::Text("%d", claimed);
        ImGui::NextColumn();
        ImGui::Text("%d", total);
        ImGui::NextColumn();
    }

    // ==================== Public API ====================

    void Render()
    {
        auto autoClaimCvar = _globalCvarManager->getCvar("acc_auto_claim");
        auto expiryClaimCvar = _globalCvarManager->getCvar("acc_claim_before_expiry");
        auto daysCvar = _globalCvarManager->getCvar("acc_days_before_expiry");

        bool autoClaimEnabled = autoClaimCvar.getBoolValue();
        bool autoClaimBeforeExpiry = expiryClaimCvar.getBoolValue();
        int  daysBeforeExpiry = daysCvar.getIntValue();

        if (ImGui::Checkbox(L("auto_claim_toggle"), &autoClaimEnabled))
        {
            autoClaimCvar.setValue(autoClaimEnabled);
            _globalCvarManager->executeCommand("writeconfig", false);
        }

        if (ImGui::Checkbox(L("auto_claim_expiry"), &autoClaimBeforeExpiry))
        {
            expiryClaimCvar.setValue(autoClaimBeforeExpiry);
            _globalCvarManager->executeCommand("writeconfig", false);
        }

        if (autoClaimBeforeExpiry)
        {
            if (ImGui::SliderInt(L("days_before_expiry"), &daysBeforeExpiry, 1, 7))
            {
                daysCvar.setValue(daysBeforeExpiry);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                _globalCvarManager->executeCommand("writeconfig", false);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!Challenges::IsManagerReady())
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
                "%s", L("enter_menu"));
            return;
        }

        ImGui::TextColored(
            ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
            "%s", L("challenge_rewards"));
        ImGui::Spacing();

        // Grid layout Columns 
        ImGui::Columns(4, "ChallengeStats", true);
        ImGui::SetColumnWidth(0, 110.0f);
        ImGui::SetColumnWidth(1, 90.0f);
        ImGui::SetColumnWidth(2, 90.0f);
        ImGui::SetColumnWidth(3, 60.0f);

        // Header
        ImGui::Text("%s", L("type"));      ImGui::NextColumn();
        ImGui::Text("%s", L("claimable")); ImGui::NextColumn();
        ImGui::Text("%s", L("claimed"));   ImGui::NextColumn();
        ImGui::Text("%s", L("total"));     ImGui::NextColumn();
        ImGui::Separator();

        // Rows
        RenderTypeRow("weekly", "WEEKLY", ImVec4(0.4f, 0.75f, 1.0f, 1.0f));
        RenderTypeRow("season", "SEASON", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        RenderTypeRow("event",  "EVENT",  ImVec4(0.95f, 0.45f, 0.95f, 1.0f));

        ImGui::Columns(1);
        ImGui::Spacing();

        // Final reward 
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
        ImGui::Text("%s", L("final"));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (Challenges::IsFinalRewardAvailable())
        {
            ImGui::Text("%s", L("season_available"));
        }
        else
        {
            ImGui::TextDisabled("%s", L("season_locked"));
        }
        ImGui::Spacing();

        // Claim button
        if (ImGui::Button(L("claim_all")))
        {
            Challenges::ClaimAll();
        }
    }
}
