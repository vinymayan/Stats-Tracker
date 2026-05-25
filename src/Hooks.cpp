#include "Hooks.h"
#include "InputEventHandler.h"
#include "Prisma.h"

namespace MenuHooks {
    bool g_bypassSystemTabHook = false;

    RE::GFxValue alphaZero(0.0);
    RE::GFxValue alphaFull(100.0);
    class JournalMenuHook {
    public:
        // 1. O ProcessMessage cuida apenas da sua interface Prisma
        static RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::JournalMenu* a_this, RE::UIMessage& a_message) {
            auto result = _ProcessMessage(a_this, a_message);

            if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
                g_bypassSystemTabHook = false;
                Prisma::Hide();
            }

            return result;
        }

        // 2. O AdvanceMovie pesca a aba exata no momento em que ela termina de renderizar
        static void AdvanceMovie_Hook(RE::JournalMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
            _AdvanceMovie(a_this, a_interval, a_currentTime);

            if (a_this->uiMovie) {
                RE::GFxValue menuMc;

                if (a_this->uiMovie->GetVariable(&menuMc, "_root.QuestJournalFader.Menu_mc")) {
                    RE::GFxValue currentTab;
                    menuMc.GetMember("iCurrentTab", &currentTab);

                    if (currentTab.IsNumber()) {
                        int tabIndex = static_cast<int>(currentTab.GetNumber());

                        // Aba 1 é a StatsTab
                        if (tabIndex == 1) {

                            // Variável estática para impedir que o menu feche antes de terminar de abrir
                            static bool isMCMFullyOpen = false;

                            // --- NOVA LÓGICA DE CHECAGEM DO MCM (USANDO _ALPHA) ---
                            if (g_bypassSystemTabHook) {
                                RE::GFxValue configPanel;

                                if (menuMc.GetMember("ConfigPanel", &configPanel) && configPanel.IsObject()) {
                                    RE::GFxValue alphaVal;

                                    // Puxa a opacidade do MCM (No ActionScript 2, vai de 0.0 a 100.0)
                                    if (configPanel.GetMember("_alpha", &alphaVal) && alphaVal.IsNumber()) {
                                        double currentAlpha = alphaVal.GetNumber();

                                        // 1. Se a opacidade passou de 90, a animação de abertura terminou (ou está quase lá)
                                        if (currentAlpha > 90.0) {
                                            isMCMFullyOpen = true;
                                        }

                                        // 2. Se o MCM JÁ ESTAVA ABERTO, e agora a opacidade caiu para perto de 0, o jogador fechou!
                                        if (isMCMFullyOpen && currentAlpha < 5.0) {
                                            g_bypassSystemTabHook = false;
                                            isMCMFullyOpen = false;
                                        }
                                    }
                                }
                            }
                            else {
                                // Garante que a flag seja resetada caso o menu inteiro seja fechado abruptamente
                                isMCMFullyOpen = false;
                            }


                            if (!g_bypassSystemTabHook) {
                                a_this->uiMovie->SetVariable("_root._alpha", alphaZero);
                                if (Prisma::IsHidden()) {
                                    Prisma::Show();
                                }
                            }
                            else {
                                a_this->uiMovie->SetVariable("_root._alpha", alphaFull);
                                if (!Prisma::IsHidden()) {
                                    Prisma::Hide();
                                }
                            }
                        }
                        else {
                            // Outras abas (Quests, Stats) - Exibe o nativo e esconde o Prisma
                            a_this->uiMovie->SetVariable("_root._alpha", alphaFull);
                            if (!Prisma::IsHidden()) {
                                Prisma::Hide();
                            }
                        }
                    }
                }
            }
        }

        static void Install() {
            REL::Relocation<std::uintptr_t> vTable(RE::VTABLE_JournalMenu[0]);
            _ProcessMessage = vTable.write_vfunc(0x4, &JournalMenuHook::ProcessMessage_Hook);
            _AdvanceMovie = vTable.write_vfunc(0x5, &JournalMenuHook::AdvanceMovie_Hook);
        }

    private:
        using ProcessMessage_t = decltype(&RE::JournalMenu::ProcessMessage);
        using AdvanceMovie_t = decltype(&RE::JournalMenu::AdvanceMovie);

        static inline REL::Relocation<ProcessMessage_t> _ProcessMessage;
        static inline REL::Relocation<AdvanceMovie_t> _AdvanceMovie;
    };

    void Install() {
        JournalMenuHook::Install();
    }
}

class Listener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    static Listener* GetSingleton() {
        static Listener singleton;
        return &singleton;
    }

    static void Register() {
        auto eventSource = SKSE::GetModCallbackEventSource();
        if (eventSource) {
            eventSource->AddEventSink(GetSingleton());
            SKSE::log::info("Listener do Input Manager Registrado e aguardando comandos!");
        }
    }

    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        std::string_view eventName = a_event->eventName.c_str();
        if (eventName == "STM_Open") {
			Prisma::Show();
            return RE::BSEventNotifyControl::kContinue;
        }


        return RE::BSEventNotifyControl::kContinue;
    }
};

struct ProcessInputQueueHook {
    static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_event) {
        a_event = InputEventHandler::Process(const_cast<RE::InputEvent**>(a_event));
        originalFunction(a_dispatcher, a_event);
    }
    static inline REL::Relocation<decltype(thunk)> originalFunction;
    static void install() {
        SKSE::AllocTrampoline(14);
        auto& trampoline = SKSE::GetTrampoline();
        originalFunction = trampoline.write_call<5>(REL::RelocationID(67315, 68617, 67315).address() + REL::Relocate(0x7B, 0x7B, 0x81), thunk);
    }
};

bool OnInput(RE::InputEvent* event) {
    if (!event) return false;
    auto button = event->AsButtonEvent();
    if (!button) return false;


    if (!Prisma::IsHidden() && !button->IsUp()) {
        auto userEvents = RE::UserEvents::GetSingleton();
        auto action = button->QUserEvent();

        // Converte o evento nativo para string_view para fazermos as comparações
        std::string_view eventName = action.c_str();

        // Mapeamento dos botões nativos para teclas direcionais enviadas à UI
        if (eventName == "Up") {
            Prisma::SendKeyPress("ArrowUp");
        }
        else if (eventName == "Down") {
            Prisma::SendKeyPress("ArrowDown");
        }
        else if (eventName == "Left") {
            Prisma::SendKeyPress("ArrowLeft");
        }
        else if (eventName == "Right") {
            Prisma::SendKeyPress("ArrowRight");
        }
        else if (eventName == "Accept") {
            Prisma::SendKeyPress("Enter");
        }
        else if (eventName == "Cancel" || eventName == "cancel") {
            Prisma::SendKeyPress("Escape");
        }
    }
    return false;
}

using namespace RE;
class MenuEvents : public RE::BSTEventSink<MenuOpenCloseEvent> {
public:
    BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent* event, BSTEventSource<MenuOpenCloseEvent>*) {
        if (event->opening && event->menuName == "PrismaUI_FocusMenu") {
            auto ui = RE::UI::GetSingleton();
            if (ui && !Prisma::IsHidden()) {
                auto focusMenu = ui->GetMenu("PrismaUI_FocusMenu");
                if (focusMenu) {
                    focusMenu->menuFlags.set(RE::UI_MENU_FLAGS::kFreezeFrameBackground, RE::UI_MENU_FLAGS::kTopmostRenderedMenu, RE::UI_MENU_FLAGS::kTopmostRenderedMenu);
                }
            }

        }
        return BSEventNotifyControl::kContinue;
    }
};

void Hooks::Install() {
    Listener::Register();
    MenuHooks::JournalMenuHook::Install();
    ProcessInputQueueHook::install();
    InputEventHandler::Register(OnInput);
    static MenuEvents menuSink;

    RE::UI::GetSingleton()->AddEventSink<MenuOpenCloseEvent>(&menuSink);
}