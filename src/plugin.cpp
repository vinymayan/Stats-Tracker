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
        if (GetModuleHandleA("TweenPause.dll")) {
            Prisma::TPM = true;
            logger::info("TweenPause.dll founded.");
        }
        else {
            Prisma::TPM = false;
            logger::info("TweenPause.dll not found.");
        }
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