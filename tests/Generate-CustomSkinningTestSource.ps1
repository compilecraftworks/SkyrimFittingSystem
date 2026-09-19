param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$groups = @{
    'DyeResourceLifecycle.production.inc' = @('src/native/FittingDye.cpp', @(
        'void UpdateWorldTintActivityLocked()', 'class WorldTintBuild final',
        '[[nodiscard]] bool IsCurrentSavedWorldTintRestore(',
        '[[nodiscard]] bool HasSavedWorldTintForActor(',
        'void ClearRuntimeWorldTintsForActor(', 'void FinishQueuedSavedWorldTintRestore(',
        'void QueueSavedWorldTintRestoreTask(', 'void QueueSavedWorldTintRestore(RE::Actor',
        'void ReleaseActorSceneResources(', 'void RestoreActorSceneResources(',
        'void ClearWorldTint()', 'void RevertSavedWorldTints()'))
    'AppearanceResourceEvents.production.inc' = @('src/native/AppearanceResourceLifecycle.cpp', @(
        'class Events final', 'void RegisterEvents()'))
    'WornSnapshot.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'class EquippedArmorSnapshot {'))
    'WornQuery.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        '[[nodiscard]] static FinalRenderedOutfitSnapshot BuildFinalRenderedOutfitSnapshot(',
        'FinalRenderedOutfitSnapshot GetFinalRenderedOutfitSnapshot(',
        '[[nodiscard]] static bool SnapshotHasBodyKeyword(',
        ('std::optional<bool>' + "`n" + 'GetDisplayedBodyKeywordState('),
        'bool IsDisplayedFittingArmor('))
    'IedEvaluationQueue.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'void ClearQueuedIedEvaluations()',
        '[[nodiscard]] bool IsActorRefreshable(',
        'class IedEvaluateCallback final',
        'void QueueIedEvaluateID(', 'void QueueIedEvaluate(RE::Actor* actor)'))
    'RenderedOutfitVisibility.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        ('[[nodiscard]] bool' + "`n" + 'IsRealArmorVisibleInDisplaySet('),
        '[[nodiscard]] std::vector<const RE::TESObjectARMO *> CollectVisibleRealArmors('))
    'RenderedOutfitBodyKeyword.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        '[[nodiscard]] static bool SnapshotHasBodyKeyword('))
    'RenderedOutfitProducer.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        ('void PrepareOutfitValue(RE::Actor* actor, const DisplaySet& display,' + "`n" +
         '    const std::unordered_set<const RE::TESObjectARMO*>& equipped) {')))
    'IntegrationOar.production.inc' = @('src/native/OpenAnimationReplacerIntegration.cpp', @(
        'bool RegisterConditionsImpl()', 'void RegisterConditions()'))
    'IntegrationDaveQuery.production.inc' = @('src/native/DaveIntegration.cpp', @(
        '[[nodiscard]] IDynamicArmorVariantsExtendedInterface001 *TryGetDaveInterface('))
    'IntegrationBackend.production.inc' = @('src/native/ArmorSkinningHooks.cpp', @(
        'bool ConfigureRealEquipmentSkinningBackend('))
    'IntegrationRaceMenu.production.inc' = @('src/native/RaceMenuBodyMorph.cpp', @(
        'void InitializeBodyMorphInterface()', 'bool IsBodyMorphInterfaceReady()'))
    'CustomSkinningCallbackFilter.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'class ScopedHiddenWornVisitorFilter final'))
    'CustomSkinningChain.production.inc' = @('src/native/ArmorSkinningHooks.cpp', @(
        '[[nodiscard]] bool IsReadableCommittedRange(',
        ('template <class T>' + "`n" + '[[nodiscard]] std::optional<T> TryReadMemory('),
        '[[nodiscard]] std::optional<ModuleChainMatch> ResolveBranchChainOwner('))
    'CustomSkinningHooks.production.inc' = @('src/native/ArmorSkinningHooks.cpp', @(
        ('[[nodiscard]] bool' + "`n" + 'ConfigureIedCustomSkinCompatibility('),
        'void InstallCustomSkinHookSE(', 'void InstallCustomSkinHookAE('))
    'CustomSkinningVisitor.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'void VisitWornItemsWithHiddenRealEquipmentFilter('))
}
foreach ($entry in $groups.GetEnumerator()) {
    $source = (Get-Content -LiteralPath (Join-Path $repo $entry.Value[0]) -Raw).Replace("`r`n", "`n")
    $blocks = [Collections.Generic.List[string]]::new()
    foreach ($name in $entry.Value[1]) {
        $start = $source.IndexOf($name, [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Missing production definition: $name" }
        $open = $source.IndexOf('{', $start)
        $depth = 0
        $end = -1
        for ($i = $open; $i -lt $source.Length; $i++) {
            if ($source[$i] -eq '{') { $depth++ }
            if ($source[$i] -eq '}') {
                $depth--
                if ($depth -eq 0) { $end = $i + 1; break }
            }
        }
        if ($end -lt 0) { throw "Unbalanced production definition: $name" }
        if ($source[$end] -eq ';') { $end++ }
        $blocks.Add($source.Substring($start, $end - $start))
    }
    # Build-generated test inputs; production functions are copied verbatim.
    $content = $blocks -join "`n`n"
    $path = [IO.Path]::GetFullPath((Join-Path $OutputDirectory $entry.Key))
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
        [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
    }
}
