// Minimal OAR Conditions API interface used by Skyrim Fitting System.
// Derived from Open Animation Replacer's public plugin API headers.
// Upstream: https://github.com/ersh1/OpenAnimationReplacer
// License: GPL-3.0-or-later with the OAR modding/linking exceptions.
#pragma once

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <cstdint>
#include <memory>
#include <string>

class IStateData {
public:
  virtual ~IStateData() = default;
};

namespace Conditions {
class ICondition;
class IConditionComponent;
class IFormConditionComponent;
class INumericConditionComponent;
class IKeywordConditionComponent;

using ConditionFactory = ICondition *(*)();
using ConditionComponentFactory = IConditionComponent *(*)(
    const ICondition *, const char *, const char *);

enum class ConditionComponentType : std::uint8_t {
  kMulti,
  kForm,
  kNumeric,
  kNiPoint3,
  kKeyword,
  kText,
  kBool,
  kComparison,
  kState,
  kCustom,
  kPreset,
};

enum class ConditionAPIVersion : std::uint8_t {
  kOld_Normal = 0,
  kOld_Custom = 1,
  kOld_Preset = 2,
  V3 = 3,
  V4 = 4,
  Latest = V4,
};

enum class ConditionType : std::uint8_t { kNormal, kCustom, kPreset };
enum class EssentialState : std::uint8_t {
  kEssential,
  kNonEssential_True,
  kNonEssential_False,
};

namespace Components {
enum class ActorValueType : int;
enum class GraphVariableType : int;
} // namespace Components

class ICondition {
public:
  virtual ~ICondition() = default;
  [[nodiscard]] virtual bool Evaluate(RE::TESObjectREFR *,
                                      RE::hkbClipGenerator *, void *) const = 0;
  virtual void Initialize(void *) = 0;
  virtual void InitializeLegacy(const char *) {}
  virtual void Serialize(void *, void *, ICondition * = nullptr) = 0;
  virtual void PreInitialize() {}
  virtual void PostInitialize() {}
  [[nodiscard]] virtual RE::BSString GetArgument() const = 0;
  [[nodiscard]] virtual RE::BSString GetCurrent(RE::TESObjectREFR *) const = 0;
  [[nodiscard]] virtual RE::BSString GetName() const = 0;
  [[nodiscard]] virtual RE::BSString GetDescription() const = 0;
  [[nodiscard]] virtual REL::Version GetRequiredVersion() const = 0;
  [[nodiscard]] virtual RE::BSString GetRequiredPluginName() const = 0;
  [[nodiscard]] virtual RE::BSString GetRequiredPluginAuthor() const = 0;
  [[nodiscard]] virtual bool IsDisabled() const = 0;
  virtual void SetDisabled(bool) = 0;
  [[nodiscard]] virtual bool IsNegated() const = 0;
  virtual void SetNegated(bool) = 0;
  [[nodiscard]] virtual bool IsValid() const { return true; }
  [[nodiscard]] virtual std::uint32_t GetNumComponents() const = 0;
  [[nodiscard]] virtual IConditionComponent *
  GetComponent(std::uint32_t) const = 0;
  virtual IConditionComponent *AddComponent(ConditionComponentFactory,
                                            const char *, const char * = "") = 0;
  [[nodiscard]] virtual ConditionAPIVersion GetConditionAPIVersion() const = 0;
  [[nodiscard]] virtual ICondition *GetWrappedCondition() const = 0;
  [[nodiscard]] virtual bool IsDeprecated() const { return false; }
  [[nodiscard]] virtual RE::TESObjectREFR *
  GetRefrToEvaluate(RE::TESObjectREFR *a_refr) const {
    return a_refr;
  }

protected:
  virtual bool EvaluateImpl(RE::TESObjectREFR *, RE::hkbClipGenerator *,
                            void *) const = 0;
  void *_parentConditionSet{nullptr};
  [[nodiscard]] virtual ConditionType GetConditionTypeImpl() const = 0;

public:
  [[nodiscard]] virtual EssentialState GetEssentialImpl() const = 0;
  virtual void SetEssential(EssentialState) = 0;
  [[nodiscard]] virtual RE::BSString GetComment() const = 0;
  virtual void SetComment(const char *) = 0;
};

class IConditionComponent {
public:
  virtual ~IConditionComponent() = default;
  virtual void InitializeComponent(void *) = 0;
  virtual void SerializeComponent(void *, void *) = 0;
  virtual void PostInitialize() {}
  virtual bool DisplayInUI(bool, float) = 0;
  [[nodiscard]] virtual ConditionComponentType GetType() const = 0;
  [[nodiscard]] virtual RE::BSString GetArgument() const = 0;
  [[nodiscard]] virtual RE::BSString GetName() const = 0;
  [[nodiscard]] virtual RE::BSString GetDescription() const = 0;
  [[nodiscard]] virtual RE::BSString GetDefaultDescription() const = 0;
  [[nodiscard]] virtual bool IsValid() const = 0;
};

class IFormConditionComponent : public IConditionComponent {
public:
  [[nodiscard]] virtual ConditionComponentType GetType() const = 0;
  [[nodiscard]] virtual RE::BSString GetDefaultDescription() const = 0;
  [[nodiscard]] virtual RE::TESForm *GetTESFormValue() const = 0;
  virtual void SetTESFormValue(RE::TESForm *) = 0;
};

// Keep this layout in sync with OAR's public INumericConditionComponent API.
// SFS only reads GetNumericValue; the remaining mutators are still declared so
// the ABI/vtable layout remains exact for the OAR component supplied at runtime.
class INumericConditionComponent : public IConditionComponent {
public:
  [[nodiscard]] virtual ConditionComponentType GetType() const = 0;
  [[nodiscard]] virtual RE::BSString GetDefaultDescription() const = 0;
  [[nodiscard]] virtual float GetNumericValue(RE::TESObjectREFR *) const = 0;
  virtual void SetStaticValue(float) = 0;
  virtual void SetGlobalVariable(RE::TESGlobal *) = 0;
  virtual void SetActorValue(RE::ActorValue, Components::ActorValueType) = 0;
  virtual void SetGraphVariable(const char *, Components::GraphVariableType) = 0;
};

class IKeywordConditionComponent : public IConditionComponent {
public:
  [[nodiscard]] virtual ConditionComponentType GetType() const = 0;
  [[nodiscard]] virtual RE::BSString GetDefaultDescription() const = 0;
  [[nodiscard]] virtual bool HasKeyword(const RE::BGSKeywordForm *) const = 0;
  virtual void SetKeyword(RE::BGSKeyword *) = 0;
  virtual void SetLiteral(const char *) = 0;
};

class CustomCondition : public ICondition {
public:
  CustomCondition();
  bool Evaluate(RE::TESObjectREFR *, RE::hkbClipGenerator *, void *) const override;
  void Initialize(void *a_value) override {
    _wrappedCondition->Initialize(a_value);
  }
  void Serialize(void *a_value, void *a_allocator,
                 ICondition *) override {
    _wrappedCondition->Serialize(a_value, a_allocator, this);
  }
  void PreInitialize() override { _wrappedCondition->PostInitialize(); }
  void PostInitialize() override { _wrappedCondition->PostInitialize(); }
  [[nodiscard]] RE::BSString GetArgument() const override {
    return _wrappedCondition->GetArgument();
  }
  [[nodiscard]] RE::BSString GetCurrent(RE::TESObjectREFR *a_refr) const override {
    return _wrappedCondition->GetCurrent(a_refr);
  }
  [[nodiscard]] RE::BSString GetRequiredPluginName() const override;
  [[nodiscard]] RE::BSString GetRequiredPluginAuthor() const override;
  [[nodiscard]] bool IsDisabled() const override {
    return _wrappedCondition->IsDisabled();
  }
  void SetDisabled(bool a_value) override {
    _wrappedCondition->SetDisabled(a_value);
  }
  [[nodiscard]] bool IsNegated() const override {
    return _wrappedCondition->IsNegated();
  }
  void SetNegated(bool a_value) override { _wrappedCondition->SetNegated(a_value); }
  [[nodiscard]] std::uint32_t GetNumComponents() const override {
    return _wrappedCondition->GetNumComponents();
  }
  [[nodiscard]] IConditionComponent *GetComponent(std::uint32_t a_index) const override {
    return _wrappedCondition->GetComponent(a_index);
  }
  IConditionComponent *AddComponent(ConditionComponentFactory a_factory,
                                    const char *a_name,
                                    const char *a_description = "") override {
    return _wrappedCondition->AddComponent(a_factory, a_name, a_description);
  }
  [[nodiscard]] ConditionAPIVersion GetConditionAPIVersion() const override {
    return ConditionAPIVersion::Latest;
  }
  [[nodiscard]] ICondition *GetWrappedCondition() const override {
    return _wrappedCondition.get();
  }
  IConditionComponent *AddBaseComponent(ConditionComponentType, const char *,
                                        const char * = "");
  template <class T> static ICondition *CreateCondition() { return new T(); }
  template <class T> static ConditionFactory GetFactory() {
    return &CreateCondition<T>;
  }

protected:
  std::unique_ptr<ICondition> _wrappedCondition;
  [[nodiscard]] ConditionType GetConditionTypeImpl() const override {
    return ConditionType::kCustom;
  }

public:
  [[nodiscard]] EssentialState GetEssentialImpl() const override {
    return _wrappedCondition->GetEssentialImpl();
  }
  void SetEssential(EssentialState a_state) override {
    _wrappedCondition->SetEssential(a_state);
  }
  [[nodiscard]] RE::BSString GetComment() const override {
    return _wrappedCondition->GetComment();
  }
  void SetComment(const char *a_comment) override {
    _wrappedCondition->SetComment(a_comment);
  }
};
} // namespace Conditions

namespace OAR_API::Conditions {
enum class InterfaceVersion : std::uint8_t { V1, V2, V3, Latest = V3 };
enum class APIResult : std::uint8_t { OK, AlreadyRegistered, Invalid, Failed };

class IConditionsInterface {
public:
  virtual APIResult AddCustomCondition(SKSE::PluginHandle, const char *,
                                       REL::Version, const char *,
                                       ::Conditions::ConditionFactory) noexcept = 0;
  virtual ::Conditions::ConditionFactory GetWrappedConditionFactory() noexcept = 0;
  virtual ::Conditions::ConditionComponentFactory
  GetConditionComponentFactory(::Conditions::ConditionComponentType) noexcept = 0;
};

using RequestPluginAPI = IConditionsInterface *(*)(InterfaceVersion,
                                                    const char *, REL::Version);
IConditionsInterface *GetAPI(InterfaceVersion = InterfaceVersion::Latest);
} // namespace OAR_API::Conditions

extern OAR_API::Conditions::IConditionsInterface *g_oarConditionsInterface;
