#include "native/SOSStorageSync.h"

#include "native/ArmorSkinning.h"
#include "native/GenitalCompatibility.h"

#include <array>
#include <atomic>
#include <memory>
#include <string_view>
#include <vector>

namespace {
constexpr std::array<std::string_view, 2> kStorageKeys{
    "SOS_RevealingArmors", "SOS_ConcealingArmors"};
constexpr std::int32_t kMaximumStorageListEntries = 4096;

struct SyncRequest {
  std::uint64_t generation{0};
  std::array<std::vector<RE::FormID>, 2> armorFormIDs;
};

std::atomic_uint64_t g_syncGeneration{0};

[[nodiscard]] bool IsCurrent(const SyncRequest &a_request) {
  return g_syncGeneration.load() == a_request.generation;
}

void DispatchListCount(const std::shared_ptr<SyncRequest> &a_request,
                       std::size_t a_listIndex);
void DispatchListEntry(const std::shared_ptr<SyncRequest> &a_request,
                       std::size_t a_listIndex, std::int32_t a_index,
                       std::int32_t a_count);

void CompleteSync(const std::shared_ptr<SyncRequest> &a_request) {
  if (!a_request || !IsCurrent(*a_request)) {
    return;
  }

  sfs::native::BeginSOSUserArmorListSync();
  for (const auto formID : a_request->armorFormIDs[0]) {
    sfs::native::AddSOSUserRevealingArmor(RE::TESForm::LookupByID(formID));
  }
  for (const auto formID : a_request->armorFormIDs[1]) {
    sfs::native::AddSOSUserConcealingArmor(RE::TESForm::LookupByID(formID));
  }
  if (sfs::native::EndSOSUserArmorListSync()) {
    sfs::native::QueuePlayerArmorRefresh();
  }
  logger::debug(
      "SFS SOS Storage sync completed revealing={} concealing={}",
      a_request->armorFormIDs[0].size(),
      a_request->armorFormIDs[1].size());
}

void FailSync(const std::shared_ptr<SyncRequest> &a_request,
              const std::string_view a_stage) {
  if (a_request && IsCurrent(*a_request)) {
    logger::debug("SFS SOS Storage sync unavailable at {}", a_stage);
  }
}

class ListEntryCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
  ListEntryCallback(std::shared_ptr<SyncRequest> a_request,
                    const std::size_t a_listIndex,
                    const std::int32_t a_index,
                    const std::int32_t a_count)
      : request_(std::move(a_request)), listIndex_(a_listIndex),
        index_(a_index), count_(a_count) {}

  void operator()(RE::BSScript::Variable a_result) override {
    if (!request_ || !IsCurrent(*request_)) {
      return;
    }
    if (auto *form = a_result.Unpack<RE::TESForm *>(); form != nullptr) {
      request_->armorFormIDs[listIndex_].push_back(form->GetFormID());
    }

    const auto nextIndex = index_ + 1;
    if (nextIndex < count_) {
      DispatchListEntry(request_, listIndex_, nextIndex, count_);
    } else if (listIndex_ + 1 < kStorageKeys.size()) {
      DispatchListCount(request_, listIndex_ + 1);
    } else {
      CompleteSync(request_);
    }
  }

  void SetObject(
      const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}

private:
  std::shared_ptr<SyncRequest> request_;
  std::size_t listIndex_{0};
  std::int32_t index_{0};
  std::int32_t count_{0};
};

class ListCountCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
  ListCountCallback(std::shared_ptr<SyncRequest> a_request,
                    const std::size_t a_listIndex)
      : request_(std::move(a_request)), listIndex_(a_listIndex) {}

  void operator()(RE::BSScript::Variable a_result) override {
    if (!request_ || !IsCurrent(*request_)) {
      return;
    }
    const auto count = a_result.IsInt()
                           ? std::clamp(a_result.GetSInt(), std::int32_t{0},
                                        kMaximumStorageListEntries)
                           : 0;
    request_->armorFormIDs[listIndex_].reserve(
        static_cast<std::size_t>(count));
    if (count > 0) {
      DispatchListEntry(request_, listIndex_, 0, count);
    } else if (listIndex_ + 1 < kStorageKeys.size()) {
      DispatchListCount(request_, listIndex_ + 1);
    } else {
      CompleteSync(request_);
    }
  }

  void SetObject(
      const RE::BSTSmartPointer<RE::BSScript::Object> &) override {}

private:
  std::shared_ptr<SyncRequest> request_;
  std::size_t listIndex_{0};
};

void DispatchListCount(const std::shared_ptr<SyncRequest> &a_request,
                       const std::size_t a_listIndex) {
  auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  if (!a_request || !IsCurrent(*a_request) || !vm ||
      a_listIndex >= kStorageKeys.size()) {
    FailSync(a_request, "FormListCount setup");
    return;
  }

  RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
      new ListCountCallback(a_request, a_listIndex));
  if (!vm->DispatchStaticCall(
          "StorageUtil", "FormListCount",
          RE::MakeFunctionArguments(static_cast<RE::TESForm *>(nullptr),
                                    std::string{kStorageKeys[a_listIndex]}),
          callback)) {
    FailSync(a_request, "FormListCount dispatch");
  }
}

void DispatchListEntry(const std::shared_ptr<SyncRequest> &a_request,
                       const std::size_t a_listIndex,
                       const std::int32_t a_index,
                       const std::int32_t a_count) {
  auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  if (!a_request || !IsCurrent(*a_request) || !vm ||
      a_listIndex >= kStorageKeys.size()) {
    FailSync(a_request, "FormListGet setup");
    return;
  }

  RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(
      new ListEntryCallback(a_request, a_listIndex, a_index, a_count));
  if (!vm->DispatchStaticCall(
          "StorageUtil", "FormListGet",
          RE::MakeFunctionArguments(static_cast<RE::TESForm *>(nullptr),
                                    std::string{kStorageKeys[a_listIndex]},
                                    static_cast<std::int32_t>(a_index)),
          callback)) {
    FailSync(a_request, "FormListGet dispatch");
  }
}
} // namespace

namespace sfs::native {
void RequestSOSStorageSync() {
  if (!sfs::native::genital_compatibility::IsSosInstalled()) {
    ++g_syncGeneration;
    ClearSOSUserArmorLists();
    logger::debug("Skipped SOS Storage sync: SOS runtime is not installed");
    return;
  }

  auto request = std::make_shared<SyncRequest>();
  request->generation = ++g_syncGeneration;
  DispatchListCount(request, 0);
}

void CancelSOSStorageSync() { ++g_syncGeneration; }
} // namespace sfs::native
