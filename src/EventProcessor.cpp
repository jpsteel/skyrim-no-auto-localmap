#include "EventProcessor.h"

#include "Hooks.h"
#include "Utility.h"

RE::BSEventNotifyControl EventProcessor::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                                      RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
    if (!a_event || a_event->menuName != RE::MapMenu::MENU_NAME) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (!a_event->opening) {
        ResetMapMenuState();
    }

    if (a_event->opening && disableButton) {
        QueueDisabledLocalMapButton();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventProcessor::ProcessEvent(RE::InputEvent* const* a_eventPtr,
                                                      RE::BSTEventSource<RE::InputEvent*>*) {
    if (!a_eventPtr || !*a_eventPtr) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* main = RE::Main::GetSingleton();

    if (!main || !main->gameActive) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* event = *a_eventPtr;

    if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* buttonEvent = event->AsButtonEvent();

    if (!buttonEvent || !buttonEvent->IsDown()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* ui = RE::UI::GetSingleton();

    if (!ui || !ui->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
        return RE::BSEventNotifyControl::kContinue;
    }

    const auto device = buttonEvent->device.get();
    HandleInputDeviceChange(device);

    const auto code = buttonEvent->GetIDCode();
    const auto userEvent = buttonEvent->QUserEvent();
    auto* userEvents = RE::UserEvents::GetSingleton();

    if ((disableAutoOpen || disableButton) && userEvents && userEvent == userEvents->cancel) {
        if (NormalizeMapBeforeMenuClose()) {
            logger::trace("[MapMenu] Map state normalized before Cancel");
        }
    }

    const auto findLocationGamepadMask = GamepadKeycodeToMask(273);

    if ((disableAutoOpen || disableButton) && device == RE::INPUT_DEVICE::kGamepad && code == findLocationGamepadMask) {
        OpenLocationFinder();

        logger::trace("[LocationFinder] Gamepad R3 detected");
    }

    return RE::BSEventNotifyControl::kContinue;
}