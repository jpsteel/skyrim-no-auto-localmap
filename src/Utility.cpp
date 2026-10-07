#include "Utility.h"

#include <spdlog/sinks/basic_file_sink.h>

bool disableAutoOpen = true;
bool disableButton = false;
bool onlyBlockInExterior = false;

bool ShouldApplyLocalMapRestrictions() {
    if (!disableAutoOpen && !disableButton) {
        return false;
    }

    if (!onlyBlockInExterior) {
        return true;
    }

    auto* player = RE::PlayerCharacter::GetSingleton();

    if (!player) {
        return false;
    }

    auto* cell = player->GetParentCell();

    if (!cell) {
        return false;
    }

    return cell->IsExteriorCell();
}

std::uint32_t GamepadKeycodeToMask(std::int32_t a_keyCode) {
    switch (a_keyCode) {
        case 266:
            return 0x0001;  // D-pad Up
        case 267:
            return 0x0002;  // D-pad Down
        case 268:
            return 0x0004;  // D-pad Left
        case 269:
            return 0x0008;  // D-pad Right
        case 270:
            return 0x0010;  // Start
        case 271:
            return 0x0020;  // Back
        case 272:
            return 0x0040;  // L3
        case 273:
            return 0x0080;  // R3
        case 274:
            return 0x0100;  // LB
        case 275:
            return 0x0200;  // RB
        case 276:
            return 0x1000;  // A
        case 277:
            return 0x2000;  // B
        case 278:
            return 0x4000;  // X
        case 279:
            return 0x8000;  // Y
        default:
            return static_cast<std::uint32_t>(a_keyCode);
    }
}

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();

    if (!logsFolder) {
        SKSE::stl::report_and_fail("SKSE log_directory not provided");
    }

    const auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();

    const auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);

    auto loggerInstance = std::make_shared<spdlog::logger>("log", std::move(fileSink));

    spdlog::set_default_logger(std::move(loggerInstance));

    spdlog::set_level(spdlog::level::trace);

    spdlog::flush_on(spdlog::level::trace);
}

void LoadDataFromINI() {
    CSimpleIniA ini;
    ini.SetUnicode();

    const auto result = ini.LoadFile(INI_FILE_PATH.c_str());

    if (result < 0) {
        logger::warn("Could not load '{}'; using defaults", INI_FILE_PATH);
    }

    disableAutoOpen = ini.GetBoolValue("MapMenu", "bDisableAutoOpen", true);
    disableButton = ini.GetBoolValue("MapMenu", "bDisableButton", false);
    onlyBlockInExterior = ini.GetBoolValue("MapMenu", "bOnlyBlockInExterior", false);

    if (disableButton) {
        disableAutoOpen = true;
    }

    logger::info(
        "Settings: bDisableAutoOpen={}, "
        "bDisableButton={}, "
        "bOnlyBlockInExterior={}",
        disableAutoOpen, disableButton, onlyBlockInExterior);
}