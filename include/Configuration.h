#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <algorithm>
#include <map>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <regex>

// Tipos base para o nosso Tracker
enum class TrackerRuleType { Global = 0, GraphVariable = 1 };
enum class GraphVarType { Bool = 0, Int = 1, Float = 2 };
enum class FloatFormat { ZeroDecimals = 0, OneDecimal = 1, TwoDecimals = 2, ThreeDecimals = 3 };

struct TrackerRule {
    std::string id;
    TrackerRuleType ruleType = TrackerRuleType::Global;

    RE::FormID attachedGlobID = 0;

    GraphVarType graphType = GraphVarType::Float;
    FloatFormat floatFormat = FloatFormat::TwoDecimals;

    std::string name;
    std::string description;
    std::string category = "General";
    std::string graphVarName;
};

namespace StatsTracker {
    inline std::map<std::string, TrackerRule> RulesDB;
    const std::string RULES_DIR = "Data/SKSE/Plugins/StatsTracker/Rules/";

    void SaveRule(const TrackerRule& rule);
    void LoadRules();

    inline std::map<std::string, float> StatValuesCache;

    // Settings (Quais estão ativos e as categorias)
    struct StatSettings {
        std::string category = "";
        bool isActive = true;
    };
    inline std::map<std::string, StatSettings> UIOptions;

    void LoadUISettings();
    void SaveUISettings();
    void FetchVanillaStatsAsync(); // A função que roda o Callback
}

namespace LocalizationManager {
    inline std::unordered_map<std::string, std::string> LangCache;
    inline bool IsLoaded = false;

    void FlattenJSON(const rapidjson::Value& value, const std::string& parentKey);
    void LoadLocalization();

    // 1. AJUSTE: Adicionado o parâmetro de fallback
    std::string T(const std::string& key, const std::string& fallback = "");
    std::string ResolveText(const std::string& text, bool isEditor);
}

namespace StatsTrackerUI {
    void GeneralMenu();
    void RegisterMenu();
    void Load();
    void Save();
}