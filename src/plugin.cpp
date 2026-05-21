#include "Plugin.h"
#include "Hooks.h"
#include "Prisma.h"
#include "Configuration.h"
#include "Manager.h"
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Manager::GetSingleton()->PopulateAllLists();
        StatsTrackerUI::RegisterMenu();
    }
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
        Prisma::Install();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SetupLog();
    logger::info("Plugin loaded");
    Hooks::Install();
    return true;
}