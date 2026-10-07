#ifndef UTILITY_H
#define UTILITY_H

#include <SimpleIni.h>

namespace logger = SKSE::log;

extern bool disableAutoOpen;
extern bool disableButton;
extern bool onlyBlockInExterior;

inline const std::string INI_FILE_PATH = "Data/SKSE/Plugins/No Auto Local Map.ini";

bool ShouldApplyLocalMapRestrictions();

std::uint32_t GamepadKeycodeToMask(std::int32_t a_keyCode);

void SetupLog();
void LoadDataFromINI();

#endif  // UTILITY_H