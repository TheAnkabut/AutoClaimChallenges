#pragma once

#include <string>
#include <unordered_map>

// Localization - Spanish/English
// Use L("key") to get translated strings

namespace Localization
{
    inline bool isSpanish = false;
    
    inline void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        isSpanish = (wrapper->GetUILanguage().ToString() == "ESN");
    }
    
    inline const char* Get(const char* key)
    {
        // Spanish
        static const std::unordered_map<std::string, const char*> ES = {
            {"join_discord", "¡Haz clic para unirte al Discord!"},
            {"title", "Auto Claim Challenges"},
            {"type", "Tipo"},
            {"auto_claim_toggle", "Reclamar automáticamente después de cada partida"},
            {"sdk_not_ready", "SDK no inicializado - entra a una partida primero"},
            {"enter_menu", "Entra al menú online para cargar desafíos"},
            {"challenge_rewards", "Recompensas de Desafíos"},
            {"weekly", "SEMANAL"},
            {"season", "TEMPORADA"},
            {"event", "EVENTO"},
            {"final", "FINAL"},
            {"total", "Total"},
            {"claimable", "Reclamable"},
            {"claimed", "Reclamado"},
            {"claim", "Reclamar"},
            {"season_available", "Incluida en reclamar todo"},
            {"season_locked", "Aún no desbloqueada"},
            {"auto_claim_expiry", "Reclamar automáticamente antes de que expiren"},
            {"days_before_expiry", "Días antes de expiración"},
            {"claim_all", "Reclamar todo"},
        };
        
        // English
        static const std::unordered_map<std::string, const char*> EN = {
            {"join_discord", "Click to join the Discord!"},
            {"title", "Auto Claim Challenges"},
            {"type", "Type"},
            {"auto_claim_toggle", "Auto-claim all rewards after every match"},
            {"sdk_not_ready", "SDK not initialized - enter a match first"},
            {"enter_menu", "Enter online menu to load challenges"},
            {"challenge_rewards", "Challenge Rewards"},
            {"weekly", "WEEKLY"},
            {"season", "SEASON"},
            {"event", "EVENT"},
            {"final", "FINAL"},
            {"total", "Total"},
            {"claimable", "Claimable"},
            {"claimed", "Claimed"},
            {"claim", "Claim"},
            {"season_available", "Included in claim all"},
            {"season_locked", "Not yet unlocked"},
            {"auto_claim_expiry", "Auto-claim before challenges expire"},
            {"days_before_expiry", "Days before expiration"},
            {"claim_all", "Claim all"},
        };
        
        const auto& dict = isSpanish ? ES : EN;
        auto it = dict.find(key);
        return (it != dict.end()) ? it->second : key;
    }
}

// Shortcut macro
#define L(key) Localization::Get(key)
