#include "Prisma.h"
#include "PrismaUI_API.h"
#include "Configuration.h"


using json = nlohmann::json;
PRISMA_UI_API::IVPrismaUI1* PrismaUI = nullptr;
static PrismaView view;

void Prisma::Install() {
    PrismaUI = reinterpret_cast<PRISMA_UI_API::IVPrismaUI1*>(PRISMA_UI_API::RequestPluginAPI());

}

void Prisma::Show() {
    if (!PrismaUI) {
        logger::error("Impossivel executar Show(): PrismaUI e nulo!");
        return;
    }


    if (!createdView) {
        createdView = true;

        #ifdef DEV_SERVER
        constexpr const char* path = "http://localhost:5174";
        #else
        constexpr const char* path = PRODUCT_NAME "/index.html";
        #endif


        view = PrismaUI->CreateView(path, [](PrismaView currentView) -> void {
            PrismaUI->RegisterJSListener(view, "hideWindow", [](const char* data) -> void {
                PrismaUI->Hide(view);
                });

            // ======= LISTENER: REQUISITAR DADOS DO STATS TRACKER =======
            PrismaUI->RegisterJSListener(currentView, "RequestTrackedStats", [](const char* data) -> void {
                StatsTracker::FetchVanillaStatsAsync();

                SKSE::GetTaskInterface()->AddTask([]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));

                    json payload = json::array();

                    for (const auto& [statName, value] : StatsTracker::StatValuesCache) {
                        if (StatsTracker::RulesDB.contains(statName)) continue;

                        json item;
                        item["id"] = statName;
                        item["name"] = LocalizationManager::T("vanilla_stats." + statName, statName);
                        item["desc"] = LocalizationManager::T("vanilla_stats_desc." + statName, "Skyrim Vanilla Stat");
                        item["value"] = value;
                        item["isCustomRule"] = false;

                        auto& opts = StatsTracker::UIOptions[statName];
                        item["isActive"] = opts.isActive;
                        item["category"] = opts.category.empty() ? "" : opts.category;

                        payload.push_back(item);
                    }

                    for (const auto& [id, rule] : StatsTracker::RulesDB) {
                        json item;
                        item["id"] = id;
                        item["name"] = LocalizationManager::ResolveText(rule.name, false);
                        item["desc"] = LocalizationManager::ResolveText(rule.description, false);
                        item["value"] = StatsTracker::StatValuesCache[id];
                        item["isCustomRule"] = true;

                        auto& opts = StatsTracker::UIOptions[id];
                        item["isActive"] = opts.isActive;

                        std::string rawCat = opts.category.empty() ? (rule.category.empty() ? "General" : rule.category) : opts.category;
                        item["category"] = rawCat;
                        item["categoryRaw"] = rawCat;

                        payload.push_back(item);
                    }

                    json response;
                    response["stats"] = payload;
                    response["customCategories"] = json::array();

                    if (StatsTracker::UIOptions.contains("__CustomCategories__")) {
                        std::string rawCats = StatsTracker::UIOptions["__CustomCategories__"].category;
                        if (!rawCats.empty() && rawCats[0] == '[') {
                            try {
                                response["customCategories"] = json::parse(rawCats);
                            }
                            catch (...) {}
                        }
                    }

                    // NOVO: Garantir carregamento prévio e injetar o dicionário completo achatado para o Front
                    if (!LocalizationManager::IsLoaded) {
                        LocalizationManager::LoadLocalization();
                    }

                    json locObj = json::object();
                    for (const auto& [key, value] : LocalizationManager::LangCache) {
                        locObj[key] = value;
                    }
                    response["localization"] = locObj;

                    std::string script = "window.dispatchEvent(new CustomEvent('OnTrackedStatsReceived', { detail: " + response.dump() + " }));";
                    PrismaUI->Invoke(view, script.c_str());
                    });
                });

            // ======= LISTENER ATUALIZADO: SALVA  =======
            PrismaUI->RegisterJSListener(currentView, "UpdateStatUISettings", [](const char* data) -> void {
                if (!data) return;
                try {
                    json jsonDoc = json::parse(data);

                    // Se o Frontend enviou a lista massiva (Array) via botão Done
                    if (jsonDoc.is_array()) {
                        for (const auto& request : jsonDoc) {
                            std::string id = request["id"];
                            if (request.contains("isActive")) StatsTracker::UIOptions[id].isActive = request["isActive"];
                            if (request.contains("category")) StatsTracker::UIOptions[id].category = request["category"];
                        }
                    }
                    // Fallback para objetos únicos individuais se necessário
                    else if (jsonDoc.is_object()) {
                        std::string id = jsonDoc["id"];
                        if (jsonDoc.contains("isActive")) StatsTracker::UIOptions[id].isActive = jsonDoc["isActive"];
                        if (jsonDoc.contains("category")) StatsTracker::UIOptions[id].category = jsonDoc["category"];
                    }

                    // Grava fisicamente no disco uma única vez
                    StatsTracker::SaveUISettings();
                }
                catch (...) {}
                });

            // ======= LISTENER: DISPARAR EVENTOS DE MENU NATIVOS (JOURNAL / PAUSE) =======
            PrismaUI->RegisterJSListener(currentView, "TriggerMenuEvent", [](const char* data) -> void {
                if (!data) return;
                try {
                    json jsonDoc = json::parse(data);
                    std::string action = jsonDoc.value("action", "");
                    if (action.empty()) return;
                    Prisma::Hide();
                   if(action == "Journal") {
                       auto q = RE::UIMessageQueue::GetSingleton();
                       if (q) {
                           q->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
                           
                       }
                   }
                    else if (action == "Pause") {
                       SKSE::GetTaskInterface()->AddUITask([]() {
                           auto dispatcher = SKSE::GetModCallbackEventSource();
                           if (dispatcher) {
                               SKSE::ModCallbackEvent modEvent{
                                   RE::BSFixedString("TPM_Open"),
                                   nullptr,
                                   0.0f,
                                   nullptr
                               };
                               dispatcher->SendEvent(&modEvent);
                           }
                           });
				   }
                    
                }
                catch (...) {}
                });

            PrismaUI->Invoke(view, "window.dispatchEvent(new CustomEvent('BackendReady'));");
            PrismaUI->Focus(view, true);
            });

        return;
    }
    PrismaUI->Show(view);
    PrismaUI->Focus(view, true);
    RE::UIBlurManager::GetSingleton()->IncrementBlurCount();
    PrismaUI->Invoke(view, "window.dispatchEvent(new CustomEvent('BackendReady'));");
}

void Prisma::Hide() {
    PrismaUI->Unfocus(view);
    PrismaUI->Hide(view); 
    RE::UIBlurManager::GetSingleton()->DecrementBlurCount();
    auto ui = RE::UI::GetSingleton();
    if (ui) {
        auto focusMenu = ui->GetMenu("PrismaUI_FocusMenu");
        if (focusMenu) {
            focusMenu->menuFlags.reset(RE::UI_MENU_FLAGS::kFreezeFrameBackground);
        }
    }
}

bool Prisma::IsHidden() { return PrismaUI->IsHidden(view); }
