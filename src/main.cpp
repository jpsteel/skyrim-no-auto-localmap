#include <spdlog/sinks/basic_file_sink.h>

#include "Utility.h"
#include "Hooks.h"
#include "EventProcessor.h"

void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
    auto eventProcessor = EventProcessor::GetSingleton();
    switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(eventProcessor);
            break;
        case SKSE::MessagingInterface::kInputLoaded:
            RE::BSInputDeviceManager::GetSingleton()->AddEventSink<RE::InputEvent*>(eventProcessor);
            break;
        default:
            break;
    }
}

extern "C" [[maybe_unused]] __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SKSE::AllocTrampoline(128);

    SetupLog();
    spdlog::set_level(spdlog::level::info);

    SKSE::GetMessagingInterface()->RegisterListener(SKSEMessageHandler);
    LoadDataFromINI();
    InstallHooks();
    logger::info("Successfully loaded NoAutoLocalMap.dll!");
    return true;
}