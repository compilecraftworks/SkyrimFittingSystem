#include "papyrus/OStimPapyrus.h"
#include "features/virtual_tokens/VirtualWornTokens.h"
#include "native/OStimIntegration.h"

namespace {
using namespace sfs::virtual_tokens;
bool Available(RE::StaticFunctionTag *) {
  sfs::native::ostim::EnablePatch();
  return true;
}
void BeginPass(RE::BSScript::IVirtualMachine *, RE::VMStackID stack,
               RE::StaticFunctionTag *, RE::Actor *actor) {
  BeginOStimEquipmentPass(stack, actor);
}
void EndPass(RE::BSScript::IVirtualMachine *, RE::VMStackID stack,
             RE::StaticFunctionTag *) { EndOStimEquipmentPass(stack); }
void Strip(RE::StaticFunctionTag *, RE::Actor *actor, std::int32_t thread,
           std::int32_t mask, bool wigs) {
  StripOStimAppearances(actor, thread, static_cast<std::uint32_t>(mask), wigs);
}
void Restore(RE::StaticFunctionTag *, RE::Actor *actor, std::int32_t thread,
             std::int32_t mask) {
  RestoreOStimAppearances(actor, thread, static_cast<std::uint32_t>(mask));
}
RE::BSFixedString Session(RE::StaticFunctionTag *, RE::Actor *actor) {
  return GetOStimRedressSession(actor);
}
std::int32_t Mask(RE::StaticFunctionTag *, RE::Actor *actor, RE::BSFixedString session) {
  return static_cast<std::int32_t>(GetOStimSessionMask(actor, session));
}
void RestoreSession(RE::StaticFunctionTag *, RE::Actor *actor,
                     RE::BSFixedString session, std::int32_t mask) {
  RestoreOStimSession(actor, session, static_cast<std::uint32_t>(mask));
}
}

namespace sfs::papyrus {
void RegisterOStim(RE::BSScript::IVirtualMachine *vm) {
  constexpr auto script = "SFSOStimBridge"sv;
  vm->RegisterFunction("IsAvailable", script, Available);
  vm->RegisterFunction("BeginEquipmentPass", script, BeginPass);
  vm->RegisterFunction("EndEquipmentPass", script, EndPass);
  vm->RegisterFunction("Strip", script, Strip);
  vm->RegisterFunction("Restore", script, Restore);
  vm->RegisterFunction("GetRedressSession", script, Session);
  vm->RegisterFunction("GetSessionMask", script, Mask);
  vm->RegisterFunction("RestoreSession", script, RestoreSession);
}
}
