// Whole production scene adapter. OS/SKSE and scene-membership boundaries are
// host fakes, not an assertion that an OStim release binary ran in game.
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
namespace RE { struct Actor { std::uint32_t id; }; }
namespace sfs::virtual_tokens {
struct Event { RE::Actor *actor; std::int32_t thread; bool start; };
std::vector<Event> events;
void ObserveOStimSceneStart(RE::Actor *actor) {
  if (actor) { events.push_back({actor, 0, true}); }
}
void ObserveOStimSceneEnd(RE::Actor *actor, std::int32_t thread) {
  if (actor) { events.push_back({actor, thread, false}); }
}
}
namespace logger { template<class... T> void info(T&&...) {} }
bool modulePresent = false;
void *GetModuleHandleW(const wchar_t *) { return modulePresent ? &modulePresent : nullptr; }
struct Messaging {
  int calls{};
  bool Dispatch(std::uint32_t, void *, std::uint32_t, const char *);
};
Messaging hostMessaging;
namespace SKSE { Messaging *GetMessagingInterface() { return &hostMessaging; } }
#include "OStimScene.production.inc"
namespace api = sfs::native::ostim::api;
struct Threads final : api::Threads {
  api::Listener *start{}, *stop{};
  int starts{}, stops{};
  std::uint32_t getVersion() override { return 0x07501020; }
  api::Thread *getThread(std::int32_t) override { return nullptr; }
  void registerThreadStartListener(api::Listener *p) override { start=p; ++starts; }
  void registerSpeedChangedListener(api::Listener *) override {}
  void registerNodeChangedListener(api::Listener *) override {}
  void registerClimaxListener(api::ActorListener *) override {}
  void registerThreadStopListener(api::Listener *p) override { stop=p; ++stops; }
} threads;
struct Map final : api::Map {
  int queries{};
  api::Plugin *queryInterface(const char *name) override {
    ++queries; return std::strcmp(name,"Threads") == 0 ? &threads : nullptr;
  }
  bool addInterface(const char *, api::Plugin *) override { return false; }
  api::Plugin *removeInterface(const char *) override { return nullptr; }
} interfaces;
bool dispatchContract = false;
bool Messaging::Dispatch(std::uint32_t type, void *data, std::uint32_t size, const char *receiver) {
  ++calls;
  dispatchContract = type == 'OST' && size == sizeof(api::Exchange) &&
      std::strcmp(receiver,"OStim") == 0;
  static_cast<api::Exchange *>(data)->interfaceMap = &interfaces;
  return dispatchContract;
}
struct Member final : api::Actor {
  RE::Actor *actor{};
  explicit Member(RE::Actor *p) : actor(p) {}
  void *getGameActor() override { return actor; }
};
struct Scene final : api::Thread {
  std::int32_t id{};
  std::vector<api::Actor *> members;
  std::int32_t getThreadID() override { return id; }
  bool isPlayerThread() override { return id==0; }
  std::uint32_t getActorCount() override { return static_cast<std::uint32_t>(members.size()); }
  api::Actor *getActor(std::uint32_t i) override { return members.at(i); }
  void forEachThreadActor(api::ActorVisitor *) override {}
  api::Node *getCurrentNode() override { return nullptr; }
};
int main() {
  int failures=0;
  const auto check=[&](bool ok,const char *why) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << why << '\n'; }
  };
  using namespace sfs::native::ostim;
  using sfs::virtual_tokens::events;
  static_assert(sizeof(api::Exchange)==sizeof(void *));
  InitializeSceneListeners();
  check(hostMessaging.calls==0,"Absent OStim must not dispatch/register");
  modulePresent=true;
  InitializeSceneListeners();
  check(dispatchContract && interfaces.queries==1,"Exact public message and interface name");
  check(threads.starts==1 && threads.stops==1,"Both v1 listener slots registered once");
  RE::Actor player{0x14}, npc{0xFF000010};
  Member first(&player), second(&npc), absent(nullptr);
  Scene scene;
  scene.members={&first,nullptr,&absent,&second};
  scene.id=0;
  threads.start->listen(&scene);
  threads.stop->listen(&scene);
  check(events.empty(),"No installed/activated patch: no appearance callbacks");
  EnablePatch();
  threads.start->listen(nullptr);
  threads.stop->listen(nullptr);
  check(events.empty(),"Null scene is harmless");
  for (int i=0;i<128;++i) {
    events.clear();
    // Above float's exact-integer range; no lossy ModEvent conversion allowed.
    scene.id=(i%2==0) ? 0 : 16777217+i;
    threads.start->listen(&scene);
    threads.stop->listen(&scene);
    check(events.size()==4,"Visit actual members only, skip missing game actors");
    if (events.size()==4) {
      check(events[0].start && events[0].actor==&player && events[1].actor==&npc,
            "Start callbacks retain actor identity");
      check(!events[2].start && events[2].thread==scene.id && events[3].thread==scene.id,
            "Stop callback preserves exact int32 thread ID");
    }
    InitializeSceneListeners();
  }
  check(hostMessaging.calls==1 && threads.starts==1 && threads.stops==1,
        "No listener growth/duplicate registration on repeated initialization");
  std::cout << "OStim scene interface checks: " << failures << " failures\n";
  return failures ? 1 : 0;
}
