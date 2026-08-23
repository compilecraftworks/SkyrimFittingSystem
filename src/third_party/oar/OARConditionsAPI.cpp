#include "third_party/oar/OARConditionsAPI.h"

#include "Plugin.h"

#include <Windows.h>

OAR_API::Conditions::IConditionsInterface *g_oarConditionsInterface = nullptr;

namespace Conditions {
CustomCondition::CustomCondition() {
  const auto factory = g_oarConditionsInterface
                           ? g_oarConditionsInterface->GetWrappedConditionFactory()
                           : nullptr;
  if (factory) {
    _wrappedCondition.reset(factory());
  }
  if (_wrappedCondition) {
    _wrappedCondition->PreInitialize();
  }
}

bool CustomCondition::Evaluate(RE::TESObjectREFR *a_refr,
                               RE::hkbClipGenerator *a_clipGenerator,
                               void *a_parentSubMod) const {
  if (!_wrappedCondition || IsDisabled()) {
    return true;
  }
  const auto result = EvaluateImpl(a_refr, a_clipGenerator, a_parentSubMod);
  return IsNegated() ? !result : result;
}

RE::BSString CustomCondition::GetRequiredPluginName() const {
  return Plugin::NAME.data();
}

RE::BSString CustomCondition::GetRequiredPluginAuthor() const {
  return Plugin::AUTHOR.data();
}

IConditionComponent *CustomCondition::AddBaseComponent(
    const ConditionComponentType a_type, const char *a_name,
    const char *a_description) {
  return g_oarConditionsInterface
             ? AddComponent(g_oarConditionsInterface->GetConditionComponentFactory(a_type),
                            a_name, a_description)
             : nullptr;
}
} // namespace Conditions

namespace OAR_API::Conditions {
IConditionsInterface *GetAPI(const InterfaceVersion a_version) {
  if (g_oarConditionsInterface) {
    return g_oarConditionsInterface;
  }
  const auto module = ::GetModuleHandleA("OpenAnimationReplacer.dll");
  if (!module) {
    return nullptr;
  }
  const auto request = reinterpret_cast<RequestPluginAPI>(
      ::GetProcAddress(module, "RequestPluginAPI_Conditions"));
  if (!request) {
    return nullptr;
  }
  g_oarConditionsInterface = request(a_version, Plugin::NAME.data(),
                                     Plugin::VERSION);
  return g_oarConditionsInterface;
}
} // namespace OAR_API::Conditions
