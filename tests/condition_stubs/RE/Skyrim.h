#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace RE {
struct BSReadWriteLock { int readers{}; };
struct BSReadLockGuard {
  BSReadWriteLock &lock;
  explicit BSReadLockGuard(BSReadWriteLock &value) : lock(value) { ++lock.readers; }
  ~BSReadLockGuard() { --lock.readers; }
};
using FormID = std::uint32_t;
struct TESForm {
  virtual ~TESForm() = default;
  FormID id{};
  std::string editorID;
  bool deleted{}, ignored{};
  template<class T> T *As() { return dynamic_cast<T *>(this); }
  FormID GetFormID() const { return id; }
  bool IsDeleted() const { return deleted; }
  bool IsIgnored() const { return ignored; }
  inline static std::map<FormID, TESForm *> byID;
  inline static BSReadWriteLock tableLock;
  static auto GetAllForms() { return std::make_pair(&byID, std::ref(tableLock)); }
  inline static std::map<std::string, TESForm *> byEditorID;
  static TESForm *LookupByID(FormID id) {
    auto it = byID.find(id); return it == byID.end() ? nullptr : it->second;
  }
  static TESForm *LookupByEditorID(const std::string &id) {
    auto it = byEditorID.find(id); return it == byEditorID.end() ? nullptr : it->second;
  }
};
struct TESObjectCELL : TESForm {};
struct TESObjectREFR : TESForm {};
struct Actor : TESObjectREFR {};
struct TESNPC : TESForm {};
struct TESRace : TESForm {};
struct TESFaction : TESForm {};
struct BGSKeyword : TESForm {};
struct BGSLocation : TESForm {};
struct TESDataHandler {
  inline static TESDataHandler *singleton{};
  static TESDataHandler *GetSingleton() { return singleton; }
  std::array<std::vector<TESForm *>, 2> formArrays;
  std::vector<TESObjectCELL *> cells, interiorCells;
  std::map<std::pair<std::string, FormID>, TESForm *> pluginForms;
  template<class T> auto &GetFormArray() {
    static_assert(std::is_same_v<T, TESObjectCELL>);
    return cells;
  }
  TESForm *LookupForm(FormID id, const std::string &plugin) {
    auto it = pluginForms.find({plugin, id});
    return it == pluginForms.end() ? nullptr : it->second;
  }
};
}
