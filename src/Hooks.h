#ifndef HOOKS_H
#define HOOKS_H

void OpenLocationFinder();
void ResetMapMenuState();
void HandleInputDeviceChange(RE::INPUT_DEVICE a_newDevice);

bool NormalizeMapBeforeMenuClose();
void QueueDisabledLocalMapButton();
void InstallHooks();

#endif  // HOOKS_H