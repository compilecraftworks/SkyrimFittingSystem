// Actual SFS IsActive hook and ENB parameter/callback adapter; recording
// providers, not a game or ENB shader execution test.
#include "ui/DepthOfFieldLease.h"
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace RE { struct ImageSpaceEffectDepthOfField { bool enabled{true}; }; }
using sfs::ui::depth_of_field::FrameLease;
#include "MenuDepthOfField.production.inc"
unsigned reads{}, writes{}, originalCalls{};
bool enabled = true, failRead{}, failWrite{}, invalidType{};
bool Original(RE::ImageSpaceEffectDepthOfField* effect) { ++originalCalls; return effect->enabled; }
void Check(bool ok, const char* message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
bool Get(const char* file, const char* section, const char* key, EnbParameter* p) {
  Check(std::strcmp(file, "enbseries.ini") == 0 && std::strcmp(section, "EFFECT") == 0 &&
        std::strcmp(key, "EnableDepthOfField") == 0, "only ENB DOF parameter is accessed");
  ++reads;
  if (failRead) return false;
  const std::int32_t value = enabled;
  p->size = 4; p->type = invalidType ? 1 : 4;
  std::memcpy(p->data, &value, 4); return true;
}
bool Set(const char*, const char*, const char*, EnbParameter* p) {
  ++writes;
  Check(p->size == 4 && p->type == 4, "ENB BOOL uses exact SDK storage, not C++ bool size");
  if (failWrite) return false;
  std::int32_t value{}; std::memcpy(&value, p->data, 4); enabled = value != 0; return true;
}
int main() {
  g_originalIsActive = Original; g_enbGet = Get; g_enbSet = Set;
  RE::ImageSpaceEffectDepthOfField effect;
  for (unsigned cycle = 0; cycle < 128; ++cycle) {
    g_menuOpen = false;
    Check(IsDepthOfFieldActive(&effect), "closed SFS preserves original DOF result");
    g_menuOpen = true;
    Check(!IsDepthOfFieldActive(&effect) && effect.enabled, "open SFS suppresses without changing engine state");
    for (unsigned frame = 0; frame < 4; ++frame) {
      EnbFrameCallback(2); Check(!enabled, "ENB DOF suppressed for rendered frame");
      EnbFrameCallback(1); Check(enabled && !g_enbFrame.PendingRestore(), "ENB original restored between frames");
    }
    EnbFrameCallback(2); g_menuOpen = false; EnbFrameCallback(1);
    Check(enabled && IsDepthOfFieldActive(&effect), "closing mid-frame restores both providers");
  }
  Check(originalCalls == 384, "original IsActive still runs exactly once per call");
  const auto idleReads = reads, idleWrites = writes;
  for (unsigned frame = 0; frame < 128; ++frame) { EnbFrameCallback(2); EnbFrameCallback(1); }
  Check(reads == idleReads && writes == idleWrites, "closed menu makes no ENB parameter calls");
  g_menuOpen = true; enabled = false; effect.enabled = false;
  EnbFrameCallback(2); EnbFrameCallback(1); g_menuOpen = false;
  Check(!enabled && !IsDepthOfFieldActive(&effect), "initially disabled effects remain disabled after closing");
  g_menuOpen = true; enabled = true;
  EnbFrameCallback(2); EnbFrameCallback(3);
  Check(enabled && !g_enbFrame.PendingRestore(), "preset save sees original value");
  EnbFrameCallback(2); enabled = false; EnbFrameCallback(4); EnbFrameCallback(1);
  Check(!enabled && !g_enbFrame.PendingRestore(), "new preset OFF supersedes old preset ON");
  enabled = true; EnbFrameCallback(2); EnbFrameCallback(7);
  Check(enabled && !g_enbFrame.PendingRestore(), "renderer reset restores parameter");
  EnbFrameCallback(2); EnbFrameCallback(6);
  Check(enabled && !g_enbFrame.PendingRestore(), "shutdown restores parameter");
  failWrite = true; EnbFrameCallback(2);
  Check(enabled && !g_enbFrame.PendingRestore(), "failed suppression never claims ownership");
  failWrite = false; EnbFrameCallback(2); failWrite = true; EnbFrameCallback(1);
  Check(g_enbFrame.PendingRestore(), "failed restore is retained");
  g_menuOpen = false; failWrite = false; EnbFrameCallback(2);
  Check(enabled && !g_enbFrame.PendingRestore(), "pending restore retries even after menu closes");
  g_menuOpen = true; invalidType = true; const auto invalidWrites = writes;
  EnbFrameCallback(2); EnbFrameCallback(1);
  Check(writes == invalidWrites, "invalid ABI data never reaches setter");
  invalidType = false; failRead = true; EnbFrameCallback(2);
  Check(!g_enbFrame.PendingRestore(), "missing ENB parameter requires no restoration");
  std::puts("Menu DOF dispatch, frame scope, lifecycle and failure checks passed (recording providers).");
}
