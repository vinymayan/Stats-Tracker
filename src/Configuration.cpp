#include "Configuration.h"
#include "Manager.h" 

namespace StatsTracker {
    void SaveRule(const TrackerRule& rule) {
        if (!std::filesystem::exists(RULES_DIR)) {
            std::filesystem::create_directories(RULES_DIR);
        }

        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        doc.AddMember("id", rapidjson::StringRef(rule.id.c_str()), alloc);
        doc.AddMember("ruleType", static_cast<int>(rule.ruleType), alloc);

        // FORM ID SAVE
        auto form = RE::TESForm::LookupByID(rule.attachedGlobID);
        std::string formStr = form ? FormUtil::NormalizeFormID(form) : "";
        rapidjson::Value globIdVal;
        globIdVal.SetString(formStr.c_str(), alloc);
        doc.AddMember("attachedGlobID", globIdVal, alloc);

        doc.AddMember("graphType", static_cast<int>(rule.graphType), alloc);
        doc.AddMember("floatFormat", static_cast<int>(rule.floatFormat), alloc);

        rapidjson::Value nameVal; nameVal.SetString(rule.name.c_str(), alloc);
        doc.AddMember("name", nameVal, alloc);

        rapidjson::Value descVal; descVal.SetString(rule.description.c_str(), alloc);
        doc.AddMember("description", descVal, alloc);

        rapidjson::Value catVal;
        std::string cleanCategory = rule.category.empty() ? "General" : rule.category;
        catVal.SetString(cleanCategory.c_str(), alloc);
        doc.AddMember("category", catVal, alloc);

        rapidjson::Value graphVarVal; graphVarVal.SetString(rule.graphVarName.c_str(), alloc);
        doc.AddMember("graphVarName", graphVarVal, alloc);

        std::string filepath = RULES_DIR + rule.id + ".json";
        std::ofstream file(filepath);
        if (file.is_open()) {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            file << buffer.GetString();
        }
    }

    void LoadRules() {
        RulesDB.clear();
        if (!std::filesystem::exists(RULES_DIR)) return;

        for (const auto& entry : std::filesystem::directory_iterator(RULES_DIR)) {
            if (entry.path().extension() == ".json") {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();

                rapidjson::Document doc;
                doc.Parse(buffer.str().c_str());
                if (doc.HasParseError() || !doc.IsObject()) continue;

                TrackerRule rule;
                if (doc.HasMember("id")) rule.id = doc["id"].GetString();
                if (doc.HasMember("ruleType")) rule.ruleType = static_cast<TrackerRuleType>(doc["ruleType"].GetInt());

                // FORM ID LOAD
                if (doc.HasMember("attachedGlobID")) {
                    if (doc["attachedGlobID"].IsString()) {
                        rule.attachedGlobID = FormUtil::FormIDFromString(doc["attachedGlobID"].GetString());
                    }
                    else if (doc["attachedGlobID"].IsUint()) {
                        rule.attachedGlobID = doc["attachedGlobID"].GetUint();
                    }
                }

                if (doc.HasMember("graphType")) rule.graphType = static_cast<GraphVarType>(doc["graphType"].GetInt());
                if (doc.HasMember("floatFormat")) rule.floatFormat = static_cast<FloatFormat>(doc["floatFormat"].GetInt());
                if (doc.HasMember("name")) rule.name = doc["name"].GetString();
                if (doc.HasMember("description")) rule.description = doc["description"].GetString();
                if (doc.HasMember("category")) rule.category = doc["category"].GetString();
                if (doc.HasMember("graphVarName") && doc["graphVarName"].IsString()) {
                    rule.graphVarName = doc["graphVarName"].GetString();
                }
                RulesDB[rule.id] = rule;
            }
        }
    }

    void LoadUISettings() {
        std::filesystem::path path("Data/SKSE/Plugins/Stats Tracker/Settings.json");
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                rapidjson::Document doc;
                std::stringstream buffer;
                buffer << file.rdbuf();
                doc.Parse(buffer.str().c_str());

                if (!doc.HasParseError() && doc.IsObject()) {
                    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
                        std::string statName = it->name.GetString();
                        if (it->value.IsObject()) {
                            UIOptions[statName].isActive = it->value.HasMember("isActive") ? it->value["isActive"].GetBool() : true;
                            UIOptions[statName].category = it->value.HasMember("category") ? it->value["category"].GetString() : "";
                        }
                    }
                }
            }
        }
    }

    void SaveUISettings() {
        std::filesystem::path dir("Data/SKSE/Plugins/Stats Tracker");
        if (!std::filesystem::exists(dir)) std::filesystem::create_directories(dir);

        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        for (const auto& [name, opts] : UIOptions) {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("isActive", opts.isActive, alloc);
            rapidjson::Value cat; cat.SetString(opts.category.c_str(), alloc);
            obj.AddMember("category", cat, alloc);

            rapidjson::Value key; key.SetString(name.c_str(), alloc);
            doc.AddMember(key, obj, alloc);
        }

        std::ofstream file(dir / "settings.json");
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        file << buffer.GetString();
    }

    struct AsyncFetchState {
        std::atomic<size_t> remaining{ 0 };
        std::function<void()> completionCallback{ nullptr };
    };

    // Callback modificado para decrementar o contador compartilhado
    class GenericStatCallback : public RE::BSScript::IStackCallbackFunctor {
        std::string _statName;
        std::shared_ptr<AsyncFetchState> _state;
    public:
        GenericStatCallback(std::string a_statName, std::shared_ptr<AsyncFetchState> a_state)
            : _statName(a_statName), _state(a_state) {
        }

        virtual void operator()(RE::BSScript::Variable a_result) override {
            float val = 0.0f;
            if (a_result.IsInt()) val = static_cast<float>(a_result.GetSInt());
            else if (a_result.IsFloat()) val = a_result.GetFloat();

            // Grava com segurança no cache estático
            StatsTracker::StatValuesCache[_statName] = val;

            // Decrementa de forma atômica. Se atingir zero, todas as estatísticas vanilla responderam.
            if (--(_state->remaining) == 0) {
                if (_state->completionCallback) {
                    _state->completionCallback();
                }
            }
        }
        virtual bool CanSave() const override { return false; }
        virtual void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    // Agora a função aceita uma função de callback (lambda) executada ao terminar tudo
    void FetchVanillaStatsAsync(std::function<void()> onComplete) {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            if (onComplete) onComplete();
            return;
        }

        const std::vector<std::string> statsToTrack = {
            "Locations Discovered", "Dungeons Cleared", "Hours Slept",
            "Hours Waiting", "Standing Stones Found", "Gold Found", "Most Gold Carried",
            "Chests Looted", "Skill Increases", "Skill Books Read", "Food Eaten",
            "Training Sessions", "Books Read", "Horses Owned", "Houses Owned",
            "Stores Invested In", "Barters", "Persuasions", "Bribes", "Intimidations",
            "Diseases Contracted", "Days as a Vampire", "Days as a Werewolf", "Necks Bitten",
            "Vampirism Cures", "Werewolf Transformations", "Mauls", "Quests Completed",
            "Misc Objectives Completed", "Main Quests Completed", "Side Quests Completed",
            "The Companions Quests Completed", "College of Winterhold Quests Completed",
            "Thieves' Guild Quests Completed", "The Dark Brotherhood Quests Completed",
            "Civil War Quests Completed", "Daedric Quests Completed", "Dawnguard Quests Completed",
            "Dragonborn Quests Completed", "Questlines Completed", "People Killed",
            "Animals Killed", "Creatures Killed", "Undead Killed", "Daedra Killed",
            "Automatons Killed", "Favorite Weapon", "Critical Strikes", "Sneak Attacks",
            "Backstabs", "Weapons Disarmed", "Brawls Won", "Bunnies Slaughtered",
            "Spells Learned", "Favorite Spell", "Favorite School", "Dragon Souls Collected",
            "Words Of Power Learned", "Words Of Power Unlocked", "Shouts Learned",
            "Shouts Unlocked", "Shouts Mastered", "Times Shouted", "Favorite Shout",
            "Soul Gems Used", "Souls Trapped", "Magic Items Made", "Weapons Improved",
            "Weapons Made", "Armor Improved", "Armor Made", "Potions Mixed",
            "Potions Used", "Poisons Mixed", "Poisons Used", "Ingredients Harvested",
            "Ingredients Eaten", "Nirnroots Found", "Wings Plucked", "Total Lifetime Bounty",
            "Largest Bounty", "Locks Picked", "Pockets Picked", "Items Pickpocketed",
            "Times Jailed", "Days Jailed", "Fines Paid", "Jail Escapes", "Items Stolen",
            "Assaults", "Murders", "Horses Stolen", "Trespasses", "Eastmarch Bounty",
            "Falkreath Bounty", "Haafingar Bounty", "Hjaalmarch Bounty", "The Pale Bounty",
            "The Reach Bounty", "The Rift Bounty", "Tribal Orcs Bounty", "Whiterun Bounty",
            "Winterhold Bounty"
        };

        // Estado compartilhado dinamicamente entre as instâncias de callback
        auto state = std::make_shared<AsyncFetchState>();
        state->remaining = statsToTrack.size();

        // Intercepta a finalização para rodar os dados síncronos e customizados antes de liberar para a UI
        state->completionCallback = [onComplete]() {
            // 1. Days Passed (via Calendar)
            if (auto calendar = RE::Calendar::GetSingleton()) {
                StatValuesCache["Days Passed"] = static_cast<float>(calendar->midnightsPassed);
            }

            // 2. Custom Rules (Globais e Graph Variables)
            for (const auto& [id, rule] : RulesDB) {
                if (rule.ruleType == TrackerRuleType::Global) {
                    auto glob = RE::TESForm::LookupByID<RE::TESGlobal>(rule.attachedGlobID);
                    if (glob) StatValuesCache[id] = glob->value;
                }
                else if (rule.ruleType == TrackerRuleType::GraphVariable) {
                    auto player = RE::PlayerCharacter::GetSingleton();
                    if (player && !rule.graphVarName.empty()) {
                        float val = 0.0f;
                        RE::BSFixedString varName(rule.graphVarName);

                        if (rule.graphType == GraphVarType::Bool) {
                            bool bVal = false;
                            if (player->GetGraphVariableBool(varName, bVal)) val = bVal ? 1.0f : 0.0f;
                        }
                        else if (rule.graphType == GraphVarType::Int) {
                            int iVal = 0;
                            if (player->GetGraphVariableInt(varName, iVal)) val = static_cast<float>(iVal);
                        }
                        else if (rule.graphType == GraphVarType::Float) {
                            float fVal = 0.0f;
                            if (player->GetGraphVariableFloat(varName, fVal)) val = fVal;
                        }
                        StatValuesCache[id] = val;
                    }
                }
            }

            // Encaminha o aviso final para quem chamou
            if (onComplete) onComplete();
            };

        size_t failures = 0;
        for (const auto& stat : statsToTrack) {
            auto args = RE::MakeFunctionArguments(RE::BSFixedString(stat));
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new GenericStatCallback(stat, state) };

            // Se o envio falhar por algum motivo interno da engine, descontamos do contador para evitar deadlock
            if (!vm->DispatchStaticCall("Game", "QueryStat", args, callback)) {
                failures++;
            }
        }

        if (failures > 0) {
            if ((state->remaining -= failures) == 0) {
                state->completionCallback();
            }
        }
    }
}

namespace LocalizationManager {
    void FlattenJSON(const rapidjson::Value& value, const std::string& parentKey) {
        if (value.IsObject()) {
            for (auto itr = value.MemberBegin(); itr != value.MemberEnd(); ++itr) {
                std::string newKey = parentKey.empty() ? itr->name.GetString() : parentKey + "." + itr->name.GetString();
                FlattenJSON(itr->value, newKey);
            }
        }
        else if (value.IsString()) {
            LangCache[parentKey] = value.GetString();
        }
    }

    void LoadLocalization() {
        LangCache.clear();
        std::string locDir = "Data/SKSE/Plugins/Stats Tracker/Localization/";

        if (std::filesystem::exists(locDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(locDir)) {
                if (entry.path().extension() == ".json") {
                    std::ifstream file(entry.path());
                    std::stringstream buffer;
                    buffer << file.rdbuf();

                    rapidjson::Document doc;
                    doc.Parse(buffer.str().c_str());
                    if (!doc.HasParseError() && doc.IsObject()) {
                        FlattenJSON(doc, "");
                    }
                }
            }
        }
        IsLoaded = true;
    }

    // 1. LÓGICA DE FALLBACK APLICADA
    std::string T(const std::string& key, const std::string& fallback) {
        if (!IsLoaded) LoadLocalization();
        auto it = LangCache.find(key);
        if (it != LangCache.end()) return it->second;
        return fallback.empty() ? key : fallback;
    }

    std::string ResolveText(const std::string& text, bool isEditor) {
        if (text.empty()) return "";
        if (isEditor) return text;

        std::regex pattern(R"(\{\{\$([a-zA-Z0-9_.]+)\}\})");
        std::smatch matches;
        std::string result = text;
        std::string::const_iterator searchStart(text.cbegin());

        std::string finalStr;
        while (std::regex_search(searchStart, text.cend(), matches, pattern)) {
            finalStr += matches.prefix();
            std::string key = matches[1].str();
            std::string translated = T(key); // Chama sem fallback, retornará a própria chave se falhar

            finalStr += (translated != key) ? translated : matches.str(0);
            searchStart = matches.suffix().first;
        }
        finalStr += std::string(searchStart, text.cend());
        return finalStr;
    }
}

namespace ImGui = ImGuiMCP;

namespace StatsTrackerUI {

    // 3. DROPDOWN (Copiado da sua referência e adaptado para buscar do Manager)
    bool DrawDropdown(const char* label, const std::string& category, RE::FormID& current_form_id, float customWidth = -1.0f) {
        bool changed = false;
        const auto& fullList = Manager::GetSingleton()->GetList(category);
        if (fullList.empty()) return false;

        std::vector<const char*> comboItems;
        std::vector<int> mapToFull;

        comboItems.push_back(LocalizationManager::T("text.none", "None").c_str());
        mapToFull.push_back(-1);

        int localSelection = 0;
        for (size_t i = 0; i < fullList.size(); ++i) {
            comboItems.push_back(fullList[i].cachedDisplayName.c_str());
            mapToFull.push_back(static_cast<int>(i));
            if (fullList[i].formID == current_form_id) {
                localSelection = static_cast<int>(i) + 1; // +1 porque o 0 é o "None"
            }
        }

        ImGui::PushID(label);
        std::string displayLabel = label;
        size_t hashPos = displayLabel.find("##");
        if (hashPos != std::string::npos) displayLabel = displayLabel.substr(0, hashPos);

        ImGui::Text("%s:", displayLabel.c_str());
        ImGui::SameLine();

        if (customWidth > 0.0f) ImGui::SetNextItemWidth(customWidth);
        const char* previewValue = comboItems[localSelection];

        if (ImGui::BeginCombo("##drop", previewValue)) {
            static std::map<std::string, std::string> searchBuffers;
            char searchBuf[256] = "";
            if (searchBuffers.contains(label)) strcpy_s(searchBuf, searchBuffers[label].c_str());

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##busca", searchBuf, sizeof(searchBuf))) {
                searchBuffers[label] = searchBuf;
            }
            ImGui::Separator();

            std::string searchStr = searchBuf;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), [](unsigned char c) { return std::tolower(c); });

            ImGui::BeginChild("##scroll", ImGui::ImVec2(0, 200), false);
            for (int i = 0; i < comboItems.size(); i++) {
                std::string itemStr = comboItems[i];
                std::string itemLower = itemStr;
                std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), [](unsigned char c) { return std::tolower(c); });

                if (searchStr.empty() || itemLower.find(searchStr) != std::string::npos) {
                    bool isSelected = (localSelection == i);
                    if (ImGui::Selectable(comboItems[i], isSelected)) {
                        localSelection = i;
                        int originalIndex = mapToFull[localSelection];

                        if (originalIndex == -1) current_form_id = 0;
                        else current_form_id = fullList[originalIndex].formID;

                        searchBuffers[label] = "";
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndChild();
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return changed;
    }


    void RenderStatsTrackerMenu() {
        std::string titleStr = LocalizationManager::T("settings.title", "Stats Tracker Editor");
        ImGui::Text("%s", titleStr.empty() ? "Stats Tracker Rules" : titleStr.c_str());
        ImGui::Separator();

        // Aplicando Fallbacks
        std::string btnAdd = "+ " + LocalizationManager::T("settings.rules.add_rule", "ADD RULE");
        if (ImGui::Button(btnAdd.c_str())) {
            TrackerRule newRule;
            newRule.id = "NovaRegra_" + std::to_string(StatsTracker::RulesDB.size() + 1);
            newRule.name = LocalizationManager::T("common.unknown", "Unknown");
            StatsTracker::RulesDB[newRule.id] = newRule;
            StatsTracker::SaveRule(newRule); // Salva no disco imediatamente ao criar
        }

        ImGui::Spacing();
        bool needsSave = false;
        std::string ruleToDelete = "";
        std::string oldIdToRename = "";
        TrackerRule ruleToRenameData;

        // Loop seguro: sem alterar a estrutura do mapa durante a iteração
        for (auto& [id, rule] : StatsTracker::RulesDB) {
            ImGui::PushID(rule.id.c_str());

            if (ImGui::CollapsingHeader(rule.id.c_str())) {
                ImGui::Indent();

                char idBuf[128];
                strcpy_s(idBuf, rule.id.c_str());
                std::string idLabel = LocalizationManager::T("header.unique_id", "ID (File Name)");
                if (ImGui::InputText(idLabel.c_str(), idBuf, sizeof(idBuf))) {
                    std::string newId(idBuf);
                    if (newId != rule.id && !newId.empty()) {
                        oldIdToRename = rule.id;
                        ruleToRenameData = rule;
                        ruleToRenameData.id = newId;
                        needsSave = true;
                    }
                }

                char nameBuf[256];
                strcpy_s(nameBuf, rule.name.c_str());
                std::string nameLabel = LocalizationManager::T("header.name", "Name");
                if (ImGui::InputText(nameLabel.c_str(), nameBuf, sizeof(nameBuf))) {
                    rule.name = nameBuf;
                    needsSave = true;
                }

                char descBuf[512];
                strcpy_s(descBuf, rule.description.c_str());
                std::string descLabel = LocalizationManager::T("header.description", "Description");
                if (ImGui::InputText(descLabel.c_str(), descBuf, sizeof(descBuf))) {
                    rule.description = descBuf;
                    needsSave = true;
                }

                char catBuf[256];
                strcpy_s(catBuf, rule.category.c_str());
                if (ImGui::InputText("Categoria Padrão", catBuf, sizeof(catBuf))) {
                    rule.category = catBuf;
                    needsSave = true;
                }

                const char* ruleTypes[] = { "Global", "Graph Variable" };
                int currentType = static_cast<int>(rule.ruleType);
                std::string typeLabel = LocalizationManager::T("header.rule_type", "Rule Type");
                if (ImGui::Combo(typeLabel.c_str(), &currentType, ruleTypes, 2)) {
                    rule.ruleType = static_cast<TrackerRuleType>(currentType);
                    needsSave = true;
                }

                ImGui::Separator();

                if (rule.ruleType == TrackerRuleType::Global) {
                    ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "Global Setup");

                    std::string dropLabel = LocalizationManager::T("settings.resources.select_glob", "Select a Global Variable");

                    RE::FormID prevGlob = rule.attachedGlobID;
                    if (DrawDropdown(dropLabel.c_str(), "Global", rule.attachedGlobID, 500.0f)) {
                        bool conflict = false;

                        if (rule.attachedGlobID != 0) {
                            for (const auto& [otherId, otherRule] : StatsTracker::RulesDB) {
                                if (otherId != rule.id &&
                                    otherRule.ruleType == TrackerRuleType::Global &&
                                    otherRule.attachedGlobID == rule.attachedGlobID) {
                                    conflict = true;
                                    break;
                                }
                            }
                        }

                        if (conflict) {
                            rule.attachedGlobID = prevGlob;
                        }
                        else {
                            needsSave = true;
                        }
                    }

                    const char* floatFormats[] = { "100", "100.0", "100.00", "100.000" };
                    int currentFmt = static_cast<int>(rule.floatFormat);
                    std::string fmtLabel = LocalizationManager::T("common.decimal_format", "Decimal Format") + "##globalFmt";
                    if (ImGui::Combo(fmtLabel.c_str(), &currentFmt, floatFormats, 4)) {
                        rule.floatFormat = static_cast<FloatFormat>(currentFmt);
                        needsSave = true;
                    }
                }
                else if (rule.ruleType == TrackerRuleType::GraphVariable) {
                    ImGui::TextColored({ 0.4f, 1.0f, 1.0f, 1.0f }, "Graph Setup");
                    char graphNameBuf[256];
                    strcpy_s(graphNameBuf, rule.graphVarName.c_str());
                    std::string graphLabel = LocalizationManager::T("header.graph_var_name", "Graph Variable Name");
                    if (ImGui::InputText(graphLabel.c_str(), graphNameBuf, sizeof(graphNameBuf))) {
                        rule.graphVarName = graphNameBuf;
                        needsSave = true;
                    }
                    const char* graphTypes[] = { "Bool", "Int", "Float" };
                    int currentGraph = static_cast<int>(rule.graphType);
                    std::string baseLabel = LocalizationManager::T("common.base_type", "Base Type");
                    if (ImGui::Combo(baseLabel.c_str(), &currentGraph, graphTypes, 3)) {
                        rule.graphType = static_cast<GraphVarType>(currentGraph);
                        needsSave = true;
                    }

                    if (rule.graphType == GraphVarType::Float) {
                        const char* floatFormats[] = { "100", "100.0", "100.00", "100.000" };
                        int currentFmt = static_cast<int>(rule.floatFormat);
                        std::string fmtLabel = LocalizationManager::T("common.decimal_format", "Decimal Format") + "##graphFmt";
                        if (ImGui::Combo(fmtLabel.c_str(), &currentFmt, floatFormats, 4)) {
                            rule.floatFormat = static_cast<FloatFormat>(currentFmt);
                            needsSave = true;
                        }
                    }
                }

                ImGui::Spacing();
                std::string btnRemove = LocalizationManager::T("common.delete", "DELETE");
                if (ImGui::Button(btnRemove.c_str(), { 150, 0 })) {
                    ruleToDelete = rule.id;
                    needsSave = true;
                }

                ImGui::Unindent();
            }
            ImGui::PopID();
        }

        // Executa as mutações estruturais de forma segura fora da iteração do loop
        if (!ruleToDelete.empty()) {
            std::string path = StatsTracker::RULES_DIR + ruleToDelete + ".json";
            if (std::filesystem::exists(path)) std::filesystem::remove(path);
            StatsTracker::RulesDB.erase(ruleToDelete);
        }

        if (!oldIdToRename.empty()) {
            std::string oldPath = StatsTracker::RULES_DIR + oldIdToRename + ".json";
            if (std::filesystem::exists(oldPath)) std::filesystem::remove(oldPath);
            StatsTracker::RulesDB.erase(oldIdToRename);
            StatsTracker::RulesDB[ruleToRenameData.id] = ruleToRenameData;
            StatsTracker::SaveRule(ruleToRenameData);
        }

        if (needsSave) {
            for (const auto& [id, rule] : StatsTracker::RulesDB) {
                StatsTracker::SaveRule(rule);
            }
        }
    }

    void RegisterMenu() {
        if (SKSEMenuFramework::IsInstalled()) {
            StatsTracker::LoadRules();
            StatsTracker::LoadUISettings();
            LocalizationManager::LoadLocalization();
            SKSEMenuFramework::SetSection("Stats Tracker");
            SKSEMenuFramework::AddSectionItem("Rules Manager", RenderStatsTrackerMenu);
        }
    }
}