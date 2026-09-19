#include "native/AppearanceResourceLifecycle.h"
#include "native/FittingDye.h"
#include "native/RaceMenuBodyMorph.h"

namespace sfs::native::appearance_resources {
namespace {
// Independent of API subscriptions and renderer backend ownership. Never
// rebuild/equip an actor from these callbacks or hold an API/workbench mutex.
class Events final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
                     public RE::BSTEventSink<RE::TESFormDeleteEvent> {
public:
  RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent *event,
      RE::BSTEventSource<RE::TESObjectLoadedEvent> *) override {
    if (event && event->formID) {
      if (event->loaded) {
        // O(1) saved-color lookup; no task or actor lookup for unrelated forms.
        dye::RestoreActorSceneResources(event->formID);
      } else {
        racemenu::ReleaseActorSceneResources(event->formID, false);
        dye::ReleaseActorSceneResources(event->formID);
      }
    }
    return RE::BSEventNotifyControl::kContinue;
  }
  RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent *event,
      RE::BSTEventSource<RE::TESFormDeleteEvent> *) override {
    if (event && event->formID) {
      // Form lookup is intentionally unnecessary after deletion.
      racemenu::ReleaseActorSceneResources(event->formID, true);
      dye::ReleaseActorSceneResources(event->formID);
    }
    return RE::BSEventNotifyControl::kContinue;
  }
};
} // namespace

void RegisterEvents() {
  static Events events;
  static bool registered = false; // DataLoaded/main thread only.
  if (registered) { return; }
  if (auto *source = RE::ScriptEventSourceHolder::GetSingleton()) {
    source->AddEventSink<RE::TESObjectLoadedEvent>(&events);
    source->AddEventSink<RE::TESFormDeleteEvent>(&events);
    registered = true;
  }
}
} // namespace sfs::native::appearance_resources
