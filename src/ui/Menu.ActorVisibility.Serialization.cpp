#include "Menu.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr std::uint32_t kActorVisibilitySerializationType = 'AHID';
constexpr std::uint32_t kActorVisibilitySerializationVersion = 1;
} // namespace

namespace sfs {
void Menu::SerializeActorVisibilitySettings(
    SKSE::SerializationInterface *a_skse) const {
  std::lock_guard lock(actorVisibilityMutex_);
  if (!a_skse) {
    return;
  }

  nlohmann::json root;
  root["actors"] = nlohmann::json::array();
  std::unordered_set<RE::FormID> actorFormIDs;
  for (const auto &[actorFormID, _] : hideRealEquipmentByActor_) {
    if (actorFormID != 0) {
      actorFormIDs.insert(actorFormID);
    }
  }
  for (const auto &[actorFormID, _] : hideFittingOverridesByActor_) {
    if (actorFormID != 0) {
      actorFormIDs.insert(actorFormID);
    }
  }

  for (const auto actorFormID : actorFormIDs) {
    const auto realIt = hideRealEquipmentByActor_.find(actorFormID);
    const auto fittingIt = hideFittingOverridesByActor_.find(actorFormID);
    root["actors"].push_back(
        {{"formID", actorFormID},
         {"hideRealEquipment",
          realIt != hideRealEquipmentByActor_.end() && realIt->second},
         {"hideFittingOverrides",
          fittingIt != hideFittingOverridesByActor_.end() &&
              fittingIt->second}});
  }
  const auto payload = root.dump();
  a_skse->WriteRecord(kActorVisibilitySerializationType,
                      kActorVisibilitySerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void Menu::DeserializeActorVisibilitySettings(
    SKSE::SerializationInterface *a_skse) {
  std::lock_guard lock(actorVisibilityMutex_);
  hideRealEquipmentByActor_.clear();
  hideFittingOverridesByActor_.clear();
  if (!a_skse) {
    return;
  }

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }

  if (type != kActorVisibilitySerializationType) {
    logger::warn("Skipping unexpected SFS actor visibility record type {:X}",
                 type);
    return;
  }

  if (version != kActorVisibilitySerializationVersion) {
    logger::warn(
        "Skipping SFS actor visibility state from unsupported version {}",
        version);
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SFS actor visibility payload");
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["actors"].is_array()) {
    logger::error("Failed to parse SFS actor visibility payload");
    return;
  }

  for (const auto &actorState : root["actors"]) {
    if (!actorState.is_object()) {
      continue;
    }

    const auto savedActorFormID = actorState.value("formID", RE::FormID{0});
    RE::FormID actorFormID = 0;
    if (savedActorFormID == 0 ||
        !a_skse->ResolveFormID(savedActorFormID, actorFormID) ||
        actorFormID == 0) {
      continue;
    }

    hideRealEquipmentByActor_[actorFormID] =
        actorState.value("hideRealEquipment", false);
    hideFittingOverridesByActor_[actorFormID] =
        actorState.value("hideFittingOverrides", false);
  }
}

void Menu::RevertActorVisibilitySettings() {
  std::lock_guard lock(actorVisibilityMutex_);
  hideRealEquipmentByActor_.clear();
  hideFittingOverridesByActor_.clear();
}
} // namespace sfs
