#pragma once
#include "api/RenderedOutfitState.h"

namespace sfs::api::rendered {
// Prepared by the existing display producer, never re-evaluated by Query.
Value CaptureValue(std::uint32_t actorFormID);
// Canonicalize caller-owned scratch in place. Copy into the cache only if the
// result changed; the caller retains its buffer capacity for the next build.
void PrepareValue(std::uint32_t actorFormID, Value& value);
void ForgetPreparedValue(std::uint32_t actorFormID);
bool HasDisplayConfiguration(std::uint32_t actorFormID);
void SetGameReady(bool ready);
void QueuePump();
void NotifyRefresh(std::uint32_t actorFormID, bool awaitsSkinning);
void NotifySkinning(std::uint32_t actorFormID);
void RegisterEvents();
// Internal IED boundary, not a public ABI relaxation. Never touches engine data
// or evaluates SFS rules. The short provider lock is released before IED runs.
std::shared_ptr<const Value> AcquirePublished(std::uint32_t actorFormID);
using DecisionObserver = void (*)(std::uint32_t actorFormID);
// Install once during PostPostLoad, before display production begins. Observes
// Ready/NotManaged decision changes only, never a same-outfit SceneChanged loop.
void SetDecisionObserver(DecisionObserver observer);
}
