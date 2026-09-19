// Audit-only host harness. Production bodies are mechanically extracted.
// Engine/VM/ImGui boundaries are fakes; this is NOT an in-game test.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "StringUtils.h"
#include "catalog/KitPathRules.h"
#include "ui/GameplayControlLease.h"

namespace logger { template <class... T> void debug(T&&...) {} }
namespace RE {
using FormID = std::uint32_t;
template <class T> using BSTSmartPointer = std::shared_ptr<T>;
struct TESForm {
  FormID id{};
  virtual ~TESForm() = default;
  FormID GetFormID() const { return id; }
  int GetFormType() const { return 0; }
  inline static std::unordered_map<FormID, TESForm*> forms;
  template <class T> static T* LookupByID(FormID id) {
    const auto it = forms.find(id);
    return it == forms.end() ? nullptr : dynamic_cast<T*>(it->second);
  }
  template <class T> static T* LookupByEditorID(const char*) { return LookupByID<T>(10); }
};
struct Actor : TESForm {};
struct TESQuest : TESForm {};
struct TESObjectARMO : TESForm {};
template <class T> int MakeFunctionArguments(T) { return 0; }
struct TESDataHandler {
  static TESDataHandler* GetSingleton() { static TESDataHandler value; return &value; }
  TESForm* LookupForm(FormID, const char*) { return TESForm::LookupByID<TESQuest>(10); }
};
namespace BSScript {
struct Object {};
struct Variable {
  TESForm* form{};
  template <class T> T Unpack() const { return dynamic_cast<T>(form); }
};
struct IStackCallbackFunctor {
  virtual ~IStackCallbackFunctor() = default;
  virtual void operator()(Variable) = 0;
  virtual void SetObject(const BSTSmartPointer<Object>&) = 0;
};
namespace Internal {
struct VirtualMachine {
  struct Policy {
    std::uint64_t GetHandleForObject(int, TESForm*) { return 1; }
    std::uint64_t EmptyHandle() { return 0; }
  } policy;
  bool acceptMethod{true}, acceptStatic{true};
  std::vector<BSTSmartPointer<IStackCallbackFunctor>> methods, statics;
  static VirtualMachine* GetSingleton() { static VirtualMachine value; return &value; }
  Policy* GetObjectHandlePolicy() { return &policy; }
  bool DispatchMethodCall(std::uint64_t, const char*, const char*, int,
                          BSTSmartPointer<IStackCallbackFunctor> cb) { if (!acceptMethod) return false; methods.push_back(cb); return true; }
  bool DispatchStaticCall(const char*, const char*, int,
                          BSTSmartPointer<IStackCallbackFunctor> cb) { if (!acceptStatic) return false; statics.push_back(cb); return true; }
};
}
}
struct ControlMap {
  enum class UEFlag : std::uint32_t { kInvalid = 0x80000000u };
  std::uint32_t enabled{0xffffffffu}, stored{0x80000000u};
  void GetControlsState(std::uint32_t& e, std::uint32_t& s) { e=enabled; s=stored; }
  struct Call { std::uint32_t flags; bool enable; bool store; };
  std::vector<Call> calls;
  static ControlMap* GetSingleton() { static ControlMap value; return &value; }
  // Conventional mask behavior modeled explicitly. Native engine not executed.
  void ToggleControls(UEFlag value, bool enable, bool store) {
    const auto flags = static_cast<std::uint32_t>(value);
    calls.push_back({flags,enable,store});
    if (enable) enabled |= flags; else enabled &= ~flags;
  }
};
}
namespace sfs::native {
inline int refreshes = 0;
void QueueArmorRefreshFor(RE::Actor*) { ++refreshes; }
namespace genital_compatibility { inline bool installed = true; bool IsSosInstalled() { return installed; } }
}
#include "Resolver.production.inc"

namespace ImGui {
struct IO { bool MouseDrawCursor{}; const char* IniFilename{}; void ClearInputKeys(){} void ClearEventsQueue(){} };
void* GetCurrentContext() { return nullptr; }
IO& GetIO() { static IO value; return value; }
void SaveIniSettingsToDisk(const char*) {}
}
// Exact mask value is irrelevant to the ownership test; named test bit is included.
constexpr std::uint32_t kBlockedGameplayControls = 0x3ffu;
using UserEventFlag = RE::ControlMap::UEFlag;
sfs::ui::GameplayControlLease g_gameplayControlLease;
#include "ControlLease.production.inc"
void AllowTextInput(RE::ControlMap*, bool) {}
namespace sfs {
namespace hooks { void ResetInputFilterState() {} }
namespace api { void SetMenuLifecycleActive(bool) {} void SetMenuInitialized(bool) {} }
namespace ui {
namespace catalog {
enum class BrowserTab { Gear, Outfits, Kits, KitGenerator };
enum class TransientPopup { None };
}
namespace components { void ClearPinnedTooltips() {} }
struct MenuCharacterPresentation {
  static MenuCharacterPresentation* GetSingleton() { static MenuCharacterPresentation value; return &value; }
  void Apply(int, RE::Actor*) {} void Restore() {}
};
}
namespace kit_generator {
struct UI { static UI& Get() { static UI value; return value; } void NotifyTabClosed() {} };
}
namespace workbench {
struct EquipmentRefreshEventSink {
  static EquipmentRefreshEventSink* GetSingleton() { static EquipmentRefreshEventSink value; return &value; }
  void QueueActorRefresh(RE::FormID) {}
};
}
struct InputManager {
  static InputManager* GetSingleton() { static InputManager value; return &value; }
  void ResetGamepadRotation() {} void SetShortcutSuppressionActive(bool) {}
};
struct Menu {
  enum class VisibilityState { Opening, Closed };
  struct Workbench { int PruneFullyEmptyConditionalRows() { return 0; } void SyncRowsFromActor(RE::Actor*) {} void ClearPreview() {} } workbench_;
  struct Browser {
    bool inventoryOnly{};
    ui::catalog::BrowserTab activeTab{ui::catalog::BrowserTab::Kits};
    std::string selectedKey;
    std::vector<std::string> selectedGearKeys;
  } browserState;
  struct CatalogDerived { int gear{}; } catalogDerived_;
  struct Pane { ui::catalog::TransientPopup activeTransientPopup{}; bool closeActiveTransientPopupRequested{}; } catalogPane_;
  bool initialized_{true}, enabled_{false}, workbenchSortDeferredUntilClose_{}, wantTextInput_{}, skyrimTextInputAllowed_{}, hideMessageQueued_{}, pendingCatalogTabSelection_{};
  int workbenchDerived_{}, workbenchActorSyncState_{}, smoothScrollWindowId_{}, menuCharacterSide_{}, focused{};
  std::vector<int> workbenchBaseSessionOrder_, workbenchConditionalSessionOrder_, pendingSlotCreations_;
  VisibilityState visibilityState_{};
  float windowAlpha_{}, pendingSmoothWheelDelta_{}, smoothScrollTargetY_{}, lastAppliedSmoothScrollY_{};
  int pendingKitListMoveDelta_{};
  bool pendingKitListApply_{}, pendingKitListPreview_{}, pendingKitListBack_{}, pendingKitListNextPane_{};
  std::chrono::steady_clock::time_point lastKitListApplyAt_{};
  Browser& CatalogBrowserState() { return browserState; }
  int& FocusedConditionEditorWindowSlot() { return focused; }
  void ApplyInitialWorkbenchFilterSelection() {}
  RE::Actor* ResolveWorkbenchPreviewActor() { return nullptr; }
  void CloseToggleKeyCapture() {}
  void SaveUserSettings() {}
  void OnMenuShow(); void OnMenuHide(); void NotifyWindowShutdown(); void ClearCatalogSelection();
};
#include "Menu.production.inc"
}
#include "Kit.production.inc"


int main(int argc, char** argv) {
  if (argc == 4) {
    const auto path=sfs::catalog::ResolveKitWritePath(argv[1],argv[2]);
    const bool expected=std::string_view(argv[3])=="allow";
    const bool passed=path.has_value()==expected;
    std::cout << "Kit filesystem containment: " << (passed?"PASSED":"FAILED") << '\n';
    return passed?0:1;
  }
  int errors = 0;
  auto check = [&](bool pass, const char* label) {
    if (!pass) { std::cerr << "FAILED: " << label << '\n'; ++errors; }
  };
  RE::Actor actor, other; actor.id=0x14; other.id=0x15;
  RE::TESQuest api, addon; api.id=10; addon.id=11;
  RE::TESObjectARMO oldArmor, newArmor; oldArmor.id=100; newArmor.id=200;
  for (RE::TESForm* form : std::array<RE::TESForm*,6>{&actor,&other,&api,&addon,&oldArmor,&newArmor}) RE::TESForm::forms[form->id]=form;
  auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
  using namespace sfs::native;
  auto reset=[&] { ClearResolvedGenitalArmors(); vm->methods.clear(); vm->statics.clear(); refreshes=0; };
  for (int round=0; round<128; ++round) {
    reset();
    RequestGenitalArmorResolution(&actor,false);
    auto oldFirst=vm->methods.back();
    (*oldFirst)({&addon}); auto oldSecond=vm->statics.back();
    ClearResolvedGenitalArmors();
    RequestGenitalArmorResolution(&actor,false);
    auto freshFirst=vm->methods.back();
    auto count=vm->statics.size();
    (*oldFirst)({&addon}); (*oldSecond)({&oldArmor});
    check(vm->statics.size()==count && GetResolvedGenitalArmor(&actor)==nullptr && refreshes==0,
          "old callbacks must not dispatch or complete while new request pending");
    (*freshFirst)({&addon}); auto freshSecond=vm->statics.back();
    (*freshSecond)({&newArmor});
    (*freshSecond)({&oldArmor}); (*oldFirst)({&addon}); (*oldSecond)({&oldArmor});
    check(GetResolvedGenitalArmor(&actor)==&newArmor && refreshes==1,
          "fresh result survives old/duplicate completions");
  }
  for (bool forget : {false,true}) {
    reset(); RequestGenitalArmorResolution(&actor,false);
    auto first=vm->methods.back(); (*first)({&addon}); auto second=vm->statics.back();
    if (forget) ForgetGenitalArmor(&actor); else RememberGenitalArmor(&actor,&newArmor);
    const auto count=vm->statics.size();
    (*first)({&addon}); (*second)({&oldArmor});
    check(vm->statics.size()==count && GetResolvedGenitalArmor(&actor)==(forget?nullptr:&newArmor),
          "explicit remember/forget owns result over stale VM callbacks");
  }
  reset();
  RequestGenitalArmorResolution(&actor,false); RequestGenitalArmorResolution(&actor,true);
  check(vm->methods.size()==1,"force must not duplicate pending request");
  (*vm->methods.back())({&addon}); (*vm->statics.back())({&newArmor});
  RequestGenitalArmorResolution(&actor,false);
  check(vm->methods.size()==1,"successful cache suppresses idle VM requests");
  RequestGenitalArmorResolution(&actor,true);
  check(vm->methods.size()==2,"force still resolves again after completion");
  RequestGenitalArmorResolution(&other,false);
  (*vm->methods.back())({&addon}); (*vm->statics.back())({&oldArmor});
  check(GetResolvedGenitalArmor(&other)==&oldArmor && GetResolvedGenitalArmor(&actor)==&newArmor,
        "actors remain independent");
  for (bool failMethod : {false,true}) {
    reset(); vm->acceptMethod=!failMethod; vm->acceptStatic=failMethod;
    RequestGenitalArmorResolution(&actor,false);
    if (!failMethod) (*vm->methods.back())({&addon});
    vm->acceptMethod=vm->acceptStatic=true;
    const auto count=vm->methods.size();
    RequestGenitalArmorResolution(&actor,true);
    check(vm->methods.size()==count+1,"failed dispatch releases pending flag for explicit retry");
    (*vm->methods.back())({&addon}); (*vm->statics.back())({&newArmor});
    check(GetResolvedGenitalArmor(&actor)==&newArmor,"retry succeeds");
  }
  reset(); genital_compatibility::installed=false;
  RequestGenitalArmorResolution(&actor,false);
  check(vm->methods.empty() && g_resolverStates.empty(),"SOS absent does no VM work or cache allocation");
  genital_compatibility::installed=true;
  RequestGenitalArmorResolution(&actor,false);
  std::weak_ptr<RE::BSScript::IStackCallbackFunctor> lifetime=vm->methods.back();
  ClearResolvedGenitalArmors(); vm->methods.clear();
  check(lifetime.expired() && g_resolverStates.empty(),"SFS retains no callback ownership after VM release");

  auto* controls=RE::ControlMap::GetSingleton();
  for (std::uint32_t preDisabled : {0u,1u,0x55u,0x3ffu}) {
    sfs::Menu menu;
    controls->enabled=0xffffffffu & ~preDisabled;
    controls->stored=0x80000000u; controls->calls.clear();
    const auto initial=controls->enabled;
    menu.OnMenuShow(); menu.OnMenuShow();
    check((controls->enabled & kBlockedGameplayControls)==0,"UI locks enabled gameplay bits");
    menu.OnMenuHide(); menu.OnMenuHide();
    check(controls->enabled==initial,"UI restores only its own bits; repeated show/hide is idempotent");
  }
  {
    sfs::Menu menu;
    controls->enabled=0xffffffffu; controls->stored=0xffffffffu;
    menu.OnMenuShow();
    controls->stored &= ~2u; // external store-state disable while open
    controls->enabled &= ~0x1000u; // unrelated bit outside SFS mask
    menu.OnMenuHide();
    check((controls->enabled&2u)==0 && (controls->enabled&0x1000u)==0,
          "close preserves current stored lock and unrelated state changes");
    check(controls->stored==(0xffffffffu & ~2u),"SFS never overwrites stored controls");
  }
  {
    sfs::Menu menu; controls->enabled=0xfffffffeu; controls->stored=0x80000000u;
    menu.OnMenuShow();
    controls->enabled |= 1u; // external owner releases its pre-existing lock
    menu.OnMenuHide();
    check(controls->enabled==0xffffffffu,"close does not reimpose an external lock released while open");
    menu.OnMenuShow(); menu.NotifyWindowShutdown(); menu.NotifyWindowShutdown();
    check(controls->enabled==0xffffffffu,"window teardown releases own controls once");
  }
  {
    sfs::Menu menu;
    menu.pendingKitListApply_=menu.pendingKitListBack_=menu.pendingKitListNextPane_=true;
    menu.OnMenuShow(); menu.OnMenuHide();
    check(!menu.pendingKitListApply_&&!menu.pendingKitListBack_&&!menu.pendingKitListNextPane_,
          "existing queued-command cleanup remains");
  }

  namespace fs=std::filesystem;
  const auto root=fs::absolute("build/kit-path-test-root").lexically_normal();
  for (const auto& relative : {fs::path("one.json"),fs::path("Armor/Heavy/one.json"),
       fs::path("./Armor/one.json"),fs::path(L"한글/中文/외형.json")}) {
    const auto path=sfs::catalog::ResolveKitWritePath(root,relative);
    if (!path) {
      std::error_code ec;
      const auto canonical=fs::weakly_canonical(root,ec);
      std::cerr << "Rejected normal root=" << root << " canonical=" << canonical
                << " error=" << ec.message() << " relative=" << relative << '\n';
    }
    check(path && *path==(root/relative).lexically_normal(),"normal nested/Unicode kit paths preserved");
  }
  for (const auto relative : {"../outside.json","a/../../outside.json","C:/outside.json",
       "C:outside.json","/outside.json","\\\\server/share/outside.json","a/file:stream.json",
       "a./file.json","a /file.json","a/../file.json","a?.json",""}) {
    check(!sfs::catalog::ResolveKitWritePath(root,fs::path(relative)),"invalid/rooted/traversal write rejected");
  }
  for (const auto collection : {"../..","C:/outside"}) {
    const auto key=fs::path(NormalizeKitCollection(collection))/"kit.json";
    check(!sfs::catalog::ResolveKitWritePath(root,key),"actual collection normalization cannot escape save root");
  }
  // Existing file paths and internal directories are accepted; no writes required.
  const auto existing=fs::absolute("tests").lexically_normal();
  check(sfs::catalog::ResolveKitWritePath(existing,"StateBoundaryTests.cpp").has_value(),
        "existing normal overwrite target remains valid");
  std::cout << "StateBoundaryTests: " << (errors ? "FAILED" : "PASSED")
            << " (128 reload cycles, SOS callbacks, control ownership, kit paths; host fakes)\n";
  return errors?1:0;
}
