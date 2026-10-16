// Real TESEquipEvent dispatch, row-hide enrollment, queue and rebuild bodies.
// Engine/ledger/render providers are recording boundaries, not an in-game test.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace RE {
using FormID = std::uint32_t;
enum class BSEventNotifyControl { kContinue };
struct TESForm {
  static inline std::unordered_map<FormID,TESForm*> forms;
  FormID id; explicit TESForm(FormID n):id(n){forms[n]=this;}
  virtual ~TESForm()=default;
  FormID GetFormID() const{return id;}
  template<class T> T* As(){return dynamic_cast<T*>(this);}
  template<class T> static T* LookupByID(FormID n){auto i=forms.find(n);return i==forms.end()?nullptr:dynamic_cast<T*>(i->second);}
};
struct Actor : TESForm { using TESForm::TESForm; };
struct PlayerCharacter { static Actor* GetSingleton(){return TESForm::LookupByID<Actor>(0x14);} };
struct TESObjectARMO : TESForm { using TESForm::TESForm; bool cover{},genital{}; };
struct Handle { Actor* actor{}; Actor* get() const{return actor;} };
struct TESEquipEvent { Handle actor; FormID baseObject{}; bool equipped{}; };
struct TESActorLocationChangeEvent {}; struct TESCellFullyLoadedEvent {}; struct TESCombatEvent {}; struct TESFastTravelEndEvent {};
template<class T> struct BSTEventSource {};
template<class T> struct BSTEventSink {virtual ~BSTEventSink()=default;virtual BSEventNotifyControl ProcessEvent(const T*,BSTEventSource<T>*)=0;};
}
namespace SKSE {
struct ModCallbackEvent {};
struct Tasks {
 std::vector<std::function<void()>> pending;
 void AddTask(std::function<void()> f){pending.push_back(std::move(f));}
 void Drain(){auto work=std::move(pending);pending.clear();for(auto& f:work)f();}
} tasks;
Tasks* GetTaskInterface(){return &tasks;}
}
namespace logger {template<class...T> void debug(T&&...){} template<class...T> void info(T&&...){} template<class...T> void warn(T&&...){} }
namespace sfs {
struct Row {RE::FormID owner{};bool hideEquipped{},slot{},alwaysVisible{};
 bool IsOwnedByActor(RE::Actor* a)const{return a&&owner==a->id;}
 bool IsSlotRow()const{return slot;} bool IsAlwaysVisibleActualEquipment()const{return alwaysVisible;}};
struct VariantWorkbench {
 mutable std::recursive_mutex mutex;
 std::vector<Row> rows_;
 std::unordered_map<RE::FormID,std::unordered_map<std::string,bool>> equippedHiddenByActor_;
 std::unordered_map<RE::FormID,std::uint64_t> links;
 auto AcquireStateLock()const{return std::unique_lock(mutex);}
 bool HasHiddenActualEquipmentForActor(RE::Actor*)const;
 bool HasDisplayStateForActor(RE::FormID)const{return false;}
 std::uint64_t GetActualEquipmentLinkedSlotMaskForActor(RE::FormID n){return links[n];}
};
struct Menu {
 VariantWorkbench workbench;
 bool ready=true; std::unordered_set<RE::FormID> globalHide; unsigned syncs{};
 static Menu* GetSingleton(){static Menu m;return &m;}
 bool IsGameDataLoaded()const{return ready;}
 VariantWorkbench& GetWorkbench(){return workbench;}
 bool HideRealEquipmentWithFittingForActor(RE::Actor* a)const{return globalHide.contains(a->id);}
 void SyncWorkbenchRowsForActor(RE::FormID){++syncs;}
};
#include "ActualEyeState.production.inc"
namespace virtual_tokens {
 bool recovery{},added{};
 void HandleVirtualWornTokenEquipEvent(RE::Actor*,RE::TESObjectARMO*,bool){}
 bool IsVirtualWornTokenEventAddedArmor(RE::FormID,RE::FormID){return added;}
 bool IsVirtualWornTokenRecoveryBurstActive(RE::FormID){return recovery;}
 void UpdateVirtualWornTokenCache(){}
 void ObserveActorContextWardrobeBoundary(RE::Actor*){}
 void RecordActorContextWardrobeSnapshot(RE::Actor*){}
}
namespace devious_devices {
 void ObserveDeviousDevicesRenderedDeviceEquipEvent(RE::Actor*,RE::TESObjectARMO*,bool){}
 void ReconcileDeviousDevicesRenderedDeviceVisibility(RE::Actor*){}
}
namespace armor {
 bool IsTngGenitalCoverArmor(const RE::TESObjectARMO* a){return a->cover;}
 bool IsSosTngGenitalArmor(const RE::TESObjectARMO* a){return a->genital;}
 const char* GetDisplayName(const RE::TESObjectARMO*){return "armor";}
 std::uint64_t GetArmorDisplaySlotMask(const RE::TESObjectARMO*){return 1;}
 namespace rules {bool ShouldRefreshForGenitalCoverEvent(bool,bool,bool,bool){return false;}}
}
namespace native {
 namespace racemenu {void ObserveHighHeelEquipmentEvent(RE::Actor*,const RE::TESObjectARMO*){}}
 std::uint64_t fitting{}; bool tracked{};
 enum class ArmorRefreshReason{kDisplayState,kEquipmentChange};
 std::vector<std::pair<RE::FormID,ArmorRefreshReason>> refreshes;
 bool HasFittingSlotState(RE::Actor*){return tracked;}
 std::uint64_t GetActiveFittingSlotMask(RE::Actor*){return fitting;}
 void SynchronizeArmorClassificationKeywords(RE::TESObjectARMO*){}
 void RememberGenitalArmor(RE::Actor*,const RE::TESObjectARMO*){}
 void ForgetGenitalArmor(RE::Actor*){} void RequestSOSStorageSync(){}
 void RefreshArmorFor(RE::Actor* a,ArmorRefreshReason r){if(a)refreshes.emplace_back(a->id,r);}
 namespace genital_compatibility {bool IsTngInstalled(){return false;}}
 namespace dave {
  std::unordered_set<RE::FormID> dirty;
  void ClearHiddenRealEquipment(RE::Actor*){}
  void MarkHiddenRealEquipmentDirty(RE::Actor* a){if(a)dirty.insert(a->id);}
 }
 namespace external_equipment {
  struct Result {bool accepted{},originalStripped{},originalItemRestored{},originalRecoveryCompleted{},eventAddedEquipped{},eventAddedRemoved{};} result;
  Result ConsumeEquipmentEvent(RE::Actor*,const RE::TESObjectARMO*,bool,std::uint64_t){return result;}
  bool IsEventAddedActualEquipment(RE::FormID,RE::FormID){return result.eventAddedEquipped;}
 }
}
}
#include "workbench/EquipmentRefreshEventSink.h"
namespace {
#include "EquipmentQueueGlobals.production.inc"
#include "EquipmentDispatchGlobals.production.inc"
void InitializeEventAddedActualVisibility(RE::Actor*,RE::FormID){}
void QueueConditionRefreshForActor(RE::Actor*){}
}
namespace sfs::workbench {
bool automatic=true;
bool IsActualEquipmentStripLinkPolicyActive(){return automatic;}
void RecordAutomaticEquipmentStateEvent(RE::FormID,RE::FormID,bool){}
std::uint64_t GetAutomaticEquipmentControlSlotMask(const RE::TESObjectARMO*){return 1;}
#include "EquipmentDispatch.production.inc"
#define EMPTY_EVENT(E) RE::BSEventNotifyControl EquipmentRefreshEventSink::ProcessEvent(const E*,RE::BSTEventSource<E>*){return RE::BSEventNotifyControl::kContinue;}
EMPTY_EVENT(RE::TESActorLocationChangeEvent) EMPTY_EVENT(RE::TESCellFullyLoadedEvent) EMPTY_EVENT(RE::TESCombatEvent) EMPTY_EVENT(RE::TESFastTravelEndEvent) EMPTY_EVENT(SKSE::ModCallbackEvent)
#undef EMPTY_EVENT
}
unsigned checks{};
void Check(bool b,const char* m){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
int main(){
 RE::Actor player{0x14},npc{0x20}; RE::TESObjectARMO helmet{0x900};
 auto* menu=sfs::Menu::GetSingleton();auto& w=menu->workbench;
 auto send=[&](RE::Actor& a,bool equipped){RE::TESEquipEvent e{{&a},helmet.id,equipped};sfs::workbench::EquipmentRefreshEventSink::GetSingleton()->ProcessEvent(&e,nullptr);};
 auto reset=[&]{SKSE::tasks.Drain();sfs::native::refreshes.clear();sfs::native::dave::dirty.clear();w.equippedHiddenByActor_.clear();w.rows_.clear();w.links.clear();menu->globalHide.clear();sfs::native::fitting=0;sfs::native::tracked=false;sfs::native::external_equipment::result={};sfs::virtual_tokens::added=false;sfs::virtual_tokens::recovery=false;sfs::workbench::automatic=true;};
 for(bool linked:{false,true})for(bool global:{false,true}){
  reset();w.links[player.id]=linked?0x80:0;
  if(global)menu->globalHide.insert(player.id);else w.equippedHiddenByActor_[player.id]["helmet"]=true;
  sfs::native::external_equipment::result.accepted=linked;
  send(player,false);send(player,true);SKSE::tasks.Drain();
  Check(sfs::native::refreshes.size()==1,"hidden actual-only/independently-linked helmet redress coalesces into one rebuild");
  Check(sfs::native::refreshes.back().second==sfs::native::ArmorRefreshReason::kEquipmentChange&&sfs::native::dave::dirty.contains(player.id),"DAV/native receives equipment reason; unchanged DAVE payload is dirtied");
  Check(w.HasHiddenActualEquipmentForActor(&player),"render repair never clears saved individual/global eyes");
 }
 reset();w.rows_.push_back({player.id,true,false,false});Check(w.HasHiddenActualEquipmentForActor(&player),"unsaved current actual row is enrolled");
 w.rows_.back().alwaysVisible=true;Check(!w.HasHiddenActualEquipmentForActor(&player),"protected always-visible row alone is not hidden");
 reset();w.equippedHiddenByActor_[player.id]["helmet"]=true;send(npc,true);SKSE::tasks.Drain();Check(sfs::native::refreshes.empty(),"another actor's eyes never enroll NPC equipment");
 for(bool equip:{false,true}){
  reset();sfs::native::fitting=4;w.links[player.id]=0x80;w.equippedHiddenByActor_[player.id]["helmet"]=true;
  auto& r=sfs::native::external_equipment::result;r.accepted=true;r.eventAddedEquipped=equip;r.eventAddedRemoved=!equip;
  send(player,equip);SKSE::tasks.Drain();Check(sfs::native::refreshes.empty(),"SGO/DD replacement prop hand-off keeps its no-extra-rebuild boundary");
 }
 reset();w.equippedHiddenByActor_[player.id]["helmet"]=true;sfs::virtual_tokens::recovery=true;
 send(player,true);Check(SKSE::tasks.pending.empty(),"mod-settings/P+/OStim recovery burst defers physical redress rebuild");
 sfs::virtual_tokens::recovery=false;sfs::workbench::EquipmentRefreshEventSink::GetSingleton()->QueueActorRefresh(player.id,false);SKSE::tasks.Drain();
 Check(sfs::native::refreshes.size()==1&&sfs::native::dave::dirty.contains(player.id),"settled display barrier preserves pending physical equipment reason");
 reset();send(player,true);SKSE::tasks.Drain();Check(sfs::native::refreshes.empty(),"unmanaged ordinary armor event does not broaden actor work");
 for(unsigned i=0;i!=128;++i){reset();w.equippedHiddenByActor_[player.id]["helmet"]=true;send(player,false);send(player,true);SKSE::tasks.Drain();Check(SKSE::tasks.pending.empty()&&g_pendingEquipmentChangeRefreshActors.empty(),"128 strip/redress cycles drain pending physical rebuilds");}
 std::printf("%u production equipment dispatch checks passed; not in-game proof.\n",checks);
}
