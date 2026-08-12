#ifndef EVENT_PROCESSOR_H
#define EVENT_PROCESSOR_H

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

class EventProcessor : public RE::BSTEventSink<RE::InputEvent*>, public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static EventProcessor* GetSingleton() {
        static EventProcessor instance;
        return std::addressof(instance);
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_eventPtr,
                                          RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;

private:
    EventProcessor() = default;
};

#endif  // EVENT_PROCESSOR_H