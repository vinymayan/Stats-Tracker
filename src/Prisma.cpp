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
                // 1. Inicia a requisição assíncrona dos valores na Engine
                StatsTracker::FetchVanillaStatsAsync();

                // 2. Aguarda a resposta da VM do Papyrus para enviar a coleção ao JS
                SKSE::GetTaskInterface()->AddTask([]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));

                    json payload = json::array();

                    // Popula as Estatísticas Vanilla (Nativas do Skyrim)
                    for (const auto& [statName, value] : StatsTracker::StatValuesCache) {
                        // Verifica se este id pertence a uma Custom Rule para não duplicar dados
                        if (StatsTracker::RulesDB.contains(statName)) continue;

                        json item;
                        item["id"] = statName;

                        // 4. CORREÇÃO: Nomes e descrições Vanilla agora são resolvidos dinamicamente pela Localização!
                        item["name"] = LocalizationManager::T("vanilla_stats." + statName, statName);
                        item["desc"] = LocalizationManager::T("vanilla_stats_desc." + statName, "Skyrim engine statistic.");

                        item["value"] = value;
                        item["isCustomRule"] = false;

                        auto& opts = StatsTracker::UIOptions[statName];
                        item["isActive"] = opts.isActive;
                        item["category"] = opts.category;

                        payload.push_back(item);
                    }

                    // Popula as Custom Rules (Criadas pelo Usuário)
                    for (const auto& [id, rule] : StatsTracker::RulesDB) {
                        json item;
                        item["id"] = id;
                        item["name"] = rule.name;
                        item["desc"] = rule.description;
                        item["value"] = StatsTracker::StatValuesCache[id];
                        item["isCustomRule"] = true;

                        auto& opts = StatsTracker::UIOptions[id];
                        item["isActive"] = opts.isActive;
                        if (opts.category.empty()) {
                            opts.category = rule.category.empty() ? "General" : rule.category;
                        }

                        // O valor final enviado será o do opts.category (que aceita as alterações do usuário no Prisma)
                        item["category"] = LocalizationManager::ResolveText(opts.category, false);

                        payload.push_back(item);
                    }

                    std::string script = "window.dispatchEvent(new CustomEvent('OnTrackedStatsReceived', { detail: " + payload.dump() + " }));";
                    PrismaUI->Invoke(view, script.c_str());
                    });
                });

            // ======= LISTENER: ATUALIZAR SETTINGS DO JSON (ATIVAR/DESATIVAR/CATEGORIA) =======
            PrismaUI->RegisterJSListener(currentView, "UpdateStatUISettings", [](const char* data) -> void {
                if (!data) return;
                try {
                    json request = json::parse(data);
                    std::string id = request["id"];

                    if (request.contains("isActive")) StatsTracker::UIOptions[id].isActive = request["isActive"];
                    if (request.contains("category")) StatsTracker::UIOptions[id].category = request["category"];

                    StatsTracker::SaveUISettings();
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
