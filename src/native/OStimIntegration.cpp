#include "native/OStimIntegration.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include <atomic>

namespace sfs::native::ostim {
namespace {
// Exact public v1 prefixes, verified at OStimNG commit
// 3954683bbdfcd34b2f9157012ed0c446d6eefd1e. No private game/thread offsets.
// getVersion() in that implementation returns the packed plugin version,
// NOT an ABI counter. Do not confuse it with the header's ABI sections.
namespace api {
class Actor { public: virtual void *getGameActor() = 0; };
class ActorVisitor;
class Node;
class Thread {
public:
  virtual std::int32_t getThreadID() = 0;
  virtual bool isPlayerThread() = 0;
  virtual std::uint32_t getActorCount() = 0;
  virtual Actor *getActor(std::uint32_t) = 0;
  virtual void forEachThreadActor(ActorVisitor *) = 0;
  virtual Node *getCurrentNode() = 0;
};
class Listener { public: virtual void listen(Thread *) = 0; };
class ActorListener;
class Plugin {
public:
  virtual ~Plugin() = default;
  virtual std::uint32_t getVersion() = 0;
};
class Threads : public Plugin {
public:
  virtual Thread *getThread(std::int32_t) = 0;
  virtual void registerThreadStartListener(Listener *) = 0;
  virtual void registerSpeedChangedListener(Listener *) = 0;
  virtual void registerNodeChangedListener(Listener *) = 0;
  virtual void registerClimaxListener(ActorListener *) = 0;
  virtual void registerThreadStopListener(Listener *) = 0;
};
class Map {
public:
  virtual Plugin *queryInterface(const char *) = 0;
  virtual bool addInterface(const char *, Plugin *) = 0;
  virtual Plugin *removeInterface(const char *) = 0;
};
struct Exchange { Map *interfaceMap{}; };
}
std::atomic_bool g_patchEnabled{false};
class SceneListener final : public api::Listener {
public:
  explicit SceneListener(bool start) : start_(start) {}
  void listen(api::Thread *thread) override {
    if (!thread || !g_patchEnabled.load(std::memory_order_acquire)) { return; }
    const auto threadID = thread->getThreadID();
    const auto count = thread->getActorCount();
    for (std::uint32_t i = 0; i < count; ++i) {
      auto *member = thread->getActor(i);
      auto *actor = member ? static_cast<RE::Actor *>(member->getGameActor()) : nullptr;
      if (start_) { virtual_tokens::ObserveOStimSceneStart(actor); }
      else { virtual_tokens::ObserveOStimSceneEnd(actor, threadID); }
    }
  }
private:
  bool start_;
};
}

void EnablePatch() { g_patchEnabled.store(true, std::memory_order_release); }

void InitializeSceneListeners() {
  static bool initialized = false; // SKSE PostPostLoad, main thread only.
  if (initialized || !GetModuleHandleW(L"OStim.dll")) { return; }
  initialized = true;
  auto *messaging = SKSE::GetMessagingInterface();
  api::Exchange exchange;
  if (!messaging || !messaging->Dispatch('OST', &exchange, sizeof(exchange), "OStim") ||
      !exchange.interfaceMap) {
    logger::info("OStim public scene interface absent; Papyrus strip/redress bridge remains available");
    return;
  }
  auto *threads = static_cast<api::Threads *>(exchange.interfaceMap->queryInterface("Threads"));
  if (!threads) { return; }
  static SceneListener start(true), stop(false);
  threads->registerThreadStartListener(&start);
  threads->registerThreadStopListener(&stop);
  logger::info("OStim official scene listeners registered (no engine hooks or actor polling)");
}
}
