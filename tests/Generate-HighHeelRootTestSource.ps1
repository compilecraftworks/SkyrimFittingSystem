param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Generate-HighHeelTestSource.ps1') -OutputDirectory $OutputDirectory
$production = Get-Content -LiteralPath (Join-Path $repo 'src/native/RaceMenuBodyMorph.cpp') -Raw
function Slice([string]$first, [string]$last) {
    $start = $production.IndexOf($first, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production start: $first" }
    $end = $production.IndexOf($last, $start + $first.Length, [StringComparison]::Ordinal)
    if ($end -le $start) { throw "Missing production end: $last" }
    return $production.Substring($start, $end - $start)
}
function Write-Generated([string]$name, [string]$content) {
    $path = [IO.Path]::GetFullPath((Join-Path $OutputDirectory $name))
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
        [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
    }
}
Write-Generated 'root-record.production.inc' (Slice 'struct RegisteredAppearanceAttachmentRoot {' 'constexpr auto kApplyBodyMorphsVtableIndex')
Write-Generated 'roots.production.inc' ((Slice 'void CollectSceneObjects(' '[[nodiscard]] bool ContainsExtraData(') +
    "`n[[nodiscard]] std::vector<RE::NiPointer<RE::NiAVObject>>`n" +
    (Slice 'FindNewAttachmentRoots(' 'enum class HighHeelSyncAttempt'))
Write-Generated 'root-lifecycle.production.inc' ((Slice 'void ReleaseActorSceneResources(' 'void ForgetAllRegisteredAppearanceNodes()') +
    (Slice 'void ForgetAllRegisteredAppearanceNodes()' '} // namespace sfs::native::racemenu'))
Write-Generated 'native-root-capture.production.inc' (Slice 'AttachmentSceneSnapshot CaptureAttachmentScene(' 'void QueueRegisteredAppearanceHighHeelSync(RE::Actor *a_actor) {')

# Reuse the recording transform/VM provider; replace only its root/selection
# stubs with actual production capture, resolution and lifecycle functions.
$fixture = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'RaceMenuHighHeelTests.cpp') -Raw
$fixture = $fixture.Substring(0, $fixture.IndexOf('int main()'))
function Replace-Exact([string]$from, [string]$to) {
    if (-not $script:fixture.Contains($from)) { throw "Missing fixture text: $from" }
    $script:fixture = $script:fixture.Replace($from, $to)
}
function Replace-Block([string]$first, [string]$last, [string]$replacement) {
    $start = $script:fixture.IndexOf($first, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing fixture start: $first" }
    $end = $script:fixture.IndexOf($last, $start + $first.Length, [StringComparison]::Ordinal)
    if ($end -le $start) { throw "Missing fixture end: $last" }
    $script:fixture = $script:fixture.Remove($start, $end - $start).Insert($start, $replacement)
}
Replace-Exact '#include <algorithm>' "#include <algorithm>`n#include <cmath>`n#include <limits>`n#include <nlohmann/json.hpp>"
Replace-Block 'template<class T> struct NiPointer {' 'struct TESForm {' "#include `"HighHeelSceneFixture.h`"`n"
Replace-Exact 'Actor* Get3D(bool) { return this; }' 'NiNode root, firstRoot; NiNode* Get3D(bool first) { return first ? &firstRoot : &root; }'
Replace-Block 'struct RegisteredHighHeelState {' 'std::mutex g_nodeMutex' ''
Replace-Block 'std::unordered_map<RE::FormID, std::vector<int>> g_registeredAppearanceAttachmentRoots;' 'void QueuePendingMorphSync(RE::FormID)' @'
#include "root-record.production.inc"
std::unordered_map<RE::FormID, std::vector<RegisteredAppearanceAttachmentRoot>> g_registeredAppearanceAttachmentRoots;
template<class T> T netimmerse_cast(RE::NiExtraData* p) { return dynamic_cast<T>(p); }
namespace sfs::native { bool IsDisplayedFittingArmor(RE::Actor*, const RE::TESObjectARMO*); }
namespace sfs::native { bool IsArmorShownForActor(RE::Actor*, const RE::TESObjectARMO*); }
#include "roots.production.inc"
bool IsRegisteredAppearanceDisplayActive(RE::FormID id) { return RE::TESForm::LookupByID<RE::Actor>(id)->active; }
std::function<void()> beforeDisplayLookup;
sfs::native::racemenu::rules::ActorMorphActivity g_morphActivity;
sfs::native::racemenu::rules::ActorMorphRequests g_morphRequests;

'@
Replace-Exact 'return a->displayed.contains(armor->id);' 'if (beforeDisplayLookup) std::exchange(beforeDisplayLookup, {})(); return a->displayed.contains(armor->id);'
$fixture += @'

void RememberAndMorphNewNodes(skee::IBodyMorphInterface*, RE::Actor*,
    const std::vector<RE::NiPointer<RE::NiAVObject>>&, RE::FormID, bool, bool, const SceneObservation*) {
  std::abort(); // Native heel capture must remain independent of BodyMorph availability.
}
namespace sfs::native::racemenu {
bool IsBodyMorphInterfaceReady() { return false; }
struct AttachmentSceneSnapshot {
  std::unordered_set<RE::NiAVObject*> thirdPersonObjects, firstPersonObjects;
};
#include "native-root-capture.production.inc"
#include "root-lifecycle.production.inc"
}
'@
Write-Generated 'HighHeelRootFixture.inc' $fixture
