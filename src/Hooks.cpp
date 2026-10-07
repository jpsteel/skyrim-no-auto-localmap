#include "Hooks.h"

#include "RE/G/GFxEvent.h"
#include "RE/G/GFxValue.h"
#include "RE/M/MapMenu.h"
#include "RE/U/UI.h"
#include "Utility.h"

namespace {
    struct RawGFxKeyEvent {
        RE::GFxEvent event;
        std::uint32_t keyCode;
        std::uint8_t asciiCode;
        std::uint8_t pad09;
        std::uint16_t pad0A;
        std::uint32_t wCharCode;
        std::uint8_t specialKeys;
        std::uint8_t keyboardIndex;
        std::uint16_t pad12;
    };

    static_assert(sizeof(RawGFxKeyEvent) == 0x14);

    bool g_locationFinderOpen = false;
    bool g_gamepadFinderOpening = false;
    bool g_localMapOpen = false;
    bool g_inputSwitchNormalizationActive = false;

    RE::INPUT_DEVICE g_lastInputDevice = RE::INPUT_DEVICE::kNone;

    bool SetBottomBarButtonVisible(RE::MapMenu* a_menu, std::uint32_t a_index, bool a_visible) {
        if (!a_menu || !a_menu->uiMovie) {
            return false;
        }

        const auto path = std::format("_root.bottomBar.buttonPanel.button{}", a_index);

        RE::GFxValue button;

        if (!a_menu->uiMovie->GetVariable(&button, path.c_str()) || !button.IsObject()) {
            logger::warn("[MapControls] Could not find bottom-bar button {}", a_index);

            return false;
        }

        RE::GFxValue visible;
        visible.SetBoolean(a_visible);

        RE::GFxValue enabled;
        enabled.SetBoolean(a_visible);

        RE::GFxValue disabled;
        disabled.SetBoolean(!a_visible);

        button.SetMember("_visible", visible);
        button.SetMember("visible", visible);
        button.SetMember("enabled", enabled);
        button.SetMember("disabled", disabled);
        button.SetMember("mouseEnabled", enabled);
        button.SetMember("focusEnabled", enabled);
        button.SetMember("tabEnabled", enabled);

        return true;
    }

    void RefreshBottomBar(RE::MapMenu* a_menu) {
        if (!a_menu || !a_menu->uiMovie) {
            return;
        }

        RE::GFxValue panel;

        if (!a_menu->uiMovie->GetVariable(&panel, "_root.bottomBar.buttonPanel") || !panel.IsObject()) {
            return;
        }

        RE::GFxValue instant;
        instant.SetBoolean(true);

        panel.Invoke("updateButtons", nullptr, &instant, 1);
    }

    void ApplyLocalMapButtonState(RE::MapMenu* a_menu, bool a_visible) {
        if (!a_menu || !disableButton || !ShouldApplyLocalMapRestrictions()) {
            return;
        }

        SetBottomBarButtonVisible(a_menu, 0, a_visible);
    }

    void RestoreDisabledBottomBar(RE::MapMenu* a_menu) {
        if (!a_menu || !disableButton || !ShouldApplyLocalMapRestrictions()) {
            return;
        }

        ApplyLocalMapButtonState(a_menu, false);

        RefreshBottomBar(a_menu);

        ApplyLocalMapButtonState(a_menu, false);
    }

    void QueueLocalMapButtonState(bool a_visible) {
        if (!disableButton || !ShouldApplyLocalMapRestrictions()) {
            return;
        }

        auto* tasks = SKSE::GetTaskInterface();

        if (!tasks) {
            logger::warn("[MapControls] UI task interface unavailable");

            return;
        }

        tasks->AddUITask([a_visible]() {
            if (!disableButton || !ShouldApplyLocalMapRestrictions()) {
                return;
            }

            auto* ui = RE::UI::GetSingleton();

            auto mapMenu = ui ? ui->GetMenu<RE::MapMenu>() : nullptr;

            if (!mapMenu) {
                return;
            }

            if (a_visible) {
                RefreshBottomBar(mapMenu.get());

                ApplyLocalMapButtonState(mapMenu.get(), true);
            } else {
                RestoreDisabledBottomBar(mapMenu.get());
            }
        });
    }

    RE::INPUT_DEVICE NormalizeInputDevice(RE::INPUT_DEVICE a_device) {
        if (a_device == RE::INPUT_DEVICE::kKeyboard || a_device == RE::INPUT_DEVICE::kMouse) {
            return RE::INPUT_DEVICE::kKeyboard;
        }

        return a_device;
    }

    int QueryLocalMapState(RE::MapMenu* a_menu) {
        if (!a_menu || !a_menu->uiMovie) {
            return -1;
        }

        RE::GFxValue state;

        if (!a_menu->uiMovie->GetVariable(&state, "_root.localMapFader.MapClip._state") || !state.IsNumber()) {
            return -1;
        }

        return static_cast<int>(state.GetNumber());
    }
}

struct InteriorLocalMapConditionHook {
    static bool thunk(void* a_state) {
        const bool result = func(a_state);

        if (result && ShouldApplyLocalMapRestrictions()) {
            logger::trace("[LocalMap] Blocked initial automatic request");

            return false;
        }

        return result;
    }

    static inline REL::Relocation<decltype(thunk)> func;
};

struct LateLocalMapAutoOpenHook {
    static std::uint64_t thunk(RE::MapMenu* a_mapMenu, bool a_showLocalMap) {
        if (a_showLocalMap && ShouldApplyLocalMapRestrictions()) {
            logger::trace("[LocalMap] Blocked late automatic request");

            return 0;
        }

        return func(a_mapMenu, a_showLocalMap);
    }

    static inline REL::Relocation<decltype(thunk)> func;
};

struct LocalMapToggleHook {
    static std::uint64_t thunk(RE::MapMenu* a_mapMenu) {
        if (!ShouldApplyLocalMapRestrictions()) {
            return func(a_mapMenu);
        }

        if (g_locationFinderOpen) {
            logger::trace(
                "[LocationFinder] Allowing native toggle to close finder");

            const auto result = func(a_mapMenu);

            g_locationFinderOpen = false;
            g_gamepadFinderOpening = false;
            g_localMapOpen = false;

            if (disableButton) {
                RestoreDisabledBottomBar(a_mapMenu);
            }

            return result;
        }

        if (disableButton) {
            logger::trace("[LocalMap] Native toggle blocked");
            return 0;
        }

        const auto result = func(a_mapMenu);

        g_localMapOpen = !g_localMapOpen;

        logger::trace(
            "[LocalMap] Manual toggle completed; localMapOpen={}",
            g_localMapOpen);

        return result;
    }

    static inline REL::Relocation<decltype(thunk)> func;
};

void HandleInputDeviceChange(RE::INPUT_DEVICE a_newDevice) {
    a_newDevice = NormalizeInputDevice(a_newDevice);

    if (!ShouldApplyLocalMapRestrictions()) {
        g_lastInputDevice = a_newDevice;
        return;
    }

    if (a_newDevice == RE::INPUT_DEVICE::kNone) {
        return;
    }

    if (g_lastInputDevice == RE::INPUT_DEVICE::kNone) {
        g_lastInputDevice = a_newDevice;
        return;
    }

    if (a_newDevice == g_lastInputDevice) {
        return;
    }

    const auto previousDevice = g_lastInputDevice;
    g_lastInputDevice = a_newDevice;

    logger::trace("[InputSwitch] Device changed from {} to {}", static_cast<std::uint32_t>(previousDevice),
                  static_cast<std::uint32_t>(a_newDevice));

    if (g_inputSwitchNormalizationActive) {
        logger::trace(
            "[InputSwitch] Normalization already active; ignoring nested switch");

        return;
    }

    const bool finderActive = g_locationFinderOpen || g_gamepadFinderOpening;

    const bool localMapActive = disableAutoOpen && !disableButton && g_localMapOpen;

    if (!finderActive && !localMapActive) {
        return;
    }

    auto* ui = RE::UI::GetSingleton();
    auto mapMenu = ui ? ui->GetMenu<RE::MapMenu>() : nullptr;

    if (!mapMenu) {
        g_locationFinderOpen = false;
        g_gamepadFinderOpening = false;
        g_localMapOpen = false;
        return;
    }

    g_inputSwitchNormalizationActive = true;

    if (finderActive) {
        logger::trace(
            "[InputSwitch] Closing Location Finder before input-mode refresh");

        LocalMapToggleHook::func(mapMenu.get());

        g_locationFinderOpen = false;
        g_gamepadFinderOpening = false;
        g_localMapOpen = false;

        if (disableButton) {
            RestoreDisabledBottomBar(mapMenu.get());
            QueueLocalMapButtonState(false);
        }
    } else if (localMapActive) {
        logger::trace(
            "[InputSwitch] Returning to World Map before input-mode refresh");

        LocalMapToggleHook::func(mapMenu.get());

        g_localMapOpen = false;
    }

    g_inputSwitchNormalizationActive = false;
}

void OpenLocationFinder() {
    if (!ShouldApplyLocalMapRestrictions()) {
        return;
    }

    g_locationFinderOpen = true;
    g_gamepadFinderOpening = true;
    g_localMapOpen = false;

    if (disableButton) {
        QueueLocalMapButtonState(true);
    }

    logger::trace("[LocationFinder] Gamepad finder opening armed");
}

void ResetMapMenuState()
{
    g_locationFinderOpen = false;
    g_gamepadFinderOpening = false;
    g_localMapOpen = false;
    g_lastInputDevice = RE::INPUT_DEVICE::kNone;
    g_inputSwitchNormalizationActive = false;
}

void QueueDisabledLocalMapButton() { QueueLocalMapButtonState(false); }

bool NormalizeMapBeforeMenuClose() {
    const bool restrictionsActive = ShouldApplyLocalMapRestrictions();

    auto* ui = RE::UI::GetSingleton();

    auto mapMenu = ui ? ui->GetMenu<RE::MapMenu>() : nullptr;

    if (!mapMenu) {
        ResetMapMenuState();
        return false;
    }

    if (!restrictionsActive && onlyBlockInExterior && disableAutoOpen) {
        auto* runtimeData = mapMenu->GetRuntimeData();

        if (runtimeData) {
            auto& localMapData = runtimeData->localMapMenu.GetRuntimeData();

            if (localMapData.showingMap) {
                logger::trace(
                    "[LocalMap] Interior map closing while Local Map active; resetting to World Map");

                LocalMapToggleHook::func(mapMenu.get());

                g_localMapOpen = false;

                return true;
            }
        }

        return false;
    }

    if (!restrictionsActive) {
        return false;
    }

    if (disableAutoOpen) {
        auto* runtimeData = mapMenu->GetRuntimeData();

        if (runtimeData) {
            auto& localMapData = runtimeData->localMapMenu.GetRuntimeData();

            if (localMapData.showingMap) {
                const bool finderWasOpen = g_locationFinderOpen;

                if (finderWasOpen) {
                    logger::trace("[LocationFinder] Native local-map mode active on close; returning to World Map");
                } else {
                    logger::trace("[LocalMap] Native local-map mode active on close; returning to World Map");
                }

                LocalMapToggleHook::func(mapMenu.get());

                g_locationFinderOpen = false;
                g_gamepadFinderOpening = false;
                g_localMapOpen = false;

                if (disableButton && finderWasOpen) {
                    RestoreDisabledBottomBar(mapMenu.get());
                }

                return true;
            }
        }
    }

    return false;
}

struct MapMenuProcessMessageHook {
    static RE::UI_MESSAGE_RESULTS thunk(RE::MapMenu* a_menu, RE::UIMessage& a_message) {
        const auto messageType = a_message.type.get();

        if (!ShouldApplyLocalMapRestrictions()) {
            const auto result = func(a_menu, a_message);

            if (messageType == RE::UI_MESSAGE_TYPE::kShow || messageType == RE::UI_MESSAGE_TYPE::kHide ||
                messageType == RE::UI_MESSAGE_TYPE::kForceHide) {
                ResetMapMenuState();
            }

            return result;
        }

        bool keyboardFinderOpening = false;

        if (messageType == RE::UI_MESSAGE_TYPE::kScaleformEvent && a_message.data) {
            auto* scaleformData = static_cast<RE::BSUIScaleformData*>(a_message.data);

            auto* event = scaleformData->scaleformEvent;

            if (event && event->type == RE::GFxEvent::EventType::kKeyDown) {
                const auto* keyEvent = reinterpret_cast<const RawGFxKeyEvent*>(event);

                if (keyEvent->keyCode == 70) {
                    keyboardFinderOpening = true;
                }
            }
        }

        if (messageType == RE::UI_MESSAGE_TYPE::kHide || messageType == RE::UI_MESSAGE_TYPE::kForceHide) {

            if (NormalizeMapBeforeMenuClose()) {
                logger::trace("[MapMenu] Map state normalized before Hide");
            }

            const auto result = func(a_menu, a_message);

            ResetMapMenuState();

            return result;
        }

        const auto result = func(a_menu, a_message);

        if (messageType == RE::UI_MESSAGE_TYPE::kShow) {
            ResetMapMenuState();

            if (disableButton) {
                QueueLocalMapButtonState(false);
            }

            return result;
        }

        if (messageType != RE::UI_MESSAGE_TYPE::kScaleformEvent) {
            return result;
        }

        const bool finderOpening = keyboardFinderOpening || g_gamepadFinderOpening;

        if (finderOpening) {
            g_locationFinderOpen = true;
            g_gamepadFinderOpening = false;
            g_localMapOpen = false;

            if (disableButton) {
                QueueLocalMapButtonState(true);
            }

            logger::trace("[LocationFinder] Finder opening event processed");

            return result;
        }

        if (g_locationFinderOpen) {
            const int localMapState = QueryLocalMapState(a_menu);

            logger::trace("[LocationFinder] Post-event LocalMap state={}", localMapState);

            // SkyUI: 0 = world map/hidden, 1 = local map, 2 = location finder
            if (localMapState == 0) {
                logger::trace("[LocationFinder] Finder ended after selection; resetting state");

                g_locationFinderOpen = false;
                g_gamepadFinderOpening = false;
                g_localMapOpen = false;

                if (disableButton) {
                    RestoreDisabledBottomBar(a_menu);
                    QueueLocalMapButtonState(false);
                }

                return result;
            }
        }

        if (!g_locationFinderOpen) {
            return result;
        }

        if (disableButton) {
            ApplyLocalMapButtonState(a_menu, true);
        }

        return result;
    }

    static inline REL::Relocation<decltype(thunk)> func;

    static void Install() {
        REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_MapMenu[0]};

        func = vtable.write_vfunc(0x04, thunk);
    }
};

void InstallHooks() {
    if (!disableAutoOpen && !disableButton) {
        logger::info("[Hooks] Both features disabled; no hooks installed");

        return;
    }

    MapMenuProcessMessageHook::Install();

    logger::info("[Hooks] MapMenu message hook installed");

    REL::Relocation<std::uintptr_t> localMapToggleCall{REL::ID(53102), 0x5A5};

    LocalMapToggleHook::func =
        SKSE::GetTrampoline().write_call<5>(localMapToggleCall.address(), LocalMapToggleHook::thunk);

    logger::info("[Hooks] Local Map toggle hook installed");

    REL::Relocation<std::uintptr_t> interiorLocalMapConditionCall{REL::ID(53119), 0xA9};

    InteriorLocalMapConditionHook::func = SKSE::GetTrampoline().write_call<5>(interiorLocalMapConditionCall.address(),
                                                                              InteriorLocalMapConditionHook::thunk);

    REL::Relocation<std::uintptr_t> lateLocalMapAutoOpenCall{REL::ID(53119), 0x11D};

    LateLocalMapAutoOpenHook::func =
        SKSE::GetTrampoline().write_call<5>(lateLocalMapAutoOpenCall.address(), LateLocalMapAutoOpenHook::thunk);

    logger::info("[Hooks] Local Map automatic-open hooks installed");

    logger::info("All enabled hooks installed");
}