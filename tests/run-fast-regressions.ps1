param(
    [string]$Xmake = "xmake"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $repository "VERSION") -Raw).Trim()
$targets = @(
    "IedConditionIntegrationTests",
    "MenuInteractionInputTests",
    "RenderedOutfitAPITests",
    "RuntimeLayoutTests",
    "KitGeneratorLogicTests",
    "BodyFamilyLogicTests",
    "ConditionCnfLogicTests",
    "ConditionDropLogicTests",
    "ConditionFormTokenTests",
    "ConditionDropdownTests",
    "ConditionStringLifetimeTests",
    "ConditionValueParsingTests",
    "FittingDyeRulesTests",
    "KitListNavigationTests",
    "CoreBehaviorRegressionTests",
    "ManualVisibilityRegressionTests",
    "CustomSkinningRegressionTests",
    "IntegrationInitializationTests",
    "RaceMenuInterfaceTests",
    "RaceMenuMorphTrackingTests",
    "RaceMenuHighHeelTests",
    "AppearanceResourceLifecycleTests",
    "CallerChainPerformanceTests",
    "WornSnapshotRegressionTests"
)

Push-Location $repository
try {
    & $Xmake -y @targets
    if ($LASTEXITCODE -ne 0) {
        throw "Regression test build failed with exit code $LASTEXITCODE"
    }

    $testDirectory = Join-Path $repository "build\v$version\tests"
    foreach ($target in $targets) {
        $test = Join-Path $testDirectory "$target.exe"
        if (-not (Test-Path -LiteralPath $test -PathType Leaf)) {
            throw "Missing regression test executable: $test"
        }
        & $test
        if ($LASTEXITCODE -ne 0) {
            throw "$target failed with exit code $LASTEXITCODE"
        }
    }

    $menuHeader = Get-Content -LiteralPath (Join-Path $repository "src/ui/Menu.h") -Raw
    $menuSettingsDefaults = Get-Content -LiteralPath (Join-Path $repository "src/ui/Menu.Settings.cpp") -Raw
    if (-not $menuHeader.Contains('bool pauseGameWhenOpen_{false}') -or
        -not $menuHeader.Contains('menuCharacterSide_{ui::MenuCharacterSide::Left}') -or
        -not $menuSettingsDefaults.Contains('json.value("pauseGameWhileOpen", pauseGameWhenOpen_)') -or
        -not $menuSettingsDefaults.Contains('"menuCharacterSide", static_cast<std::uint8_t>(menuCharacterSide_)')) {
        throw "First-run defaults must be left/unpaused, while existing saved choices remain authoritative"
    }
    $sfsInput = Get-Content -LiteralPath (Join-Path $repository "src/InputManager.cpp") -Raw
    $sfsInputHooks = Get-Content -LiteralPath (Join-Path $repository "src/Hooks.cpp") -Raw
    if (-not $sfsInput.Contains('input::IsMenuCancel(*buttonEvent)') -or
        -not $sfsInputHooks.Contains('sfs::input::IsMenuCancel(*buttonEvent)') -or
        $sfsInputHooks.IndexOf('AddEventToQueue(a_events)') -gt $sfsInputHooks.IndexOf('FilterBlockedInputEvents(a_events);') -or
        -not $sfsInput.Contains('gamepadRotation_.SetRightX(stick->xValue)') -or
        -not $sfsInput.Contains('RE::INPUT_EVENT_TYPE::kDeviceConnect')) {
        throw "Menu Cancel must share one mapping/filter and analog rotation must copy input before filtering"
    }
    foreach ($sfsLocale in @('en', 'kor', 'zh_cn')) {
        $sfsStrings = (Get-Content -LiteralPath (Join-Path $repository "data/Interface/SkyrimFittingSystem/locales/$sfsLocale.json") -Raw | ConvertFrom-Json).strings
        if (-not $sfsStrings.'window.rotation_hint' -or -not $sfsStrings.'window.rotation_hint_compact') {
            throw "Rotation title hint is missing for $sfsLocale"
        }
    }
    $conditionLowering = Get-Content -LiteralPath (
        Join-Path $repository "src/conditions/Lowering.cpp") -Raw
    if ($conditionLowering -notmatch 'case ParamType::kActor:\s*\{[^}]*As<RE::Actor>' -or
        $conditionLowering -notmatch 'case ParamType::kActorBase:\s*case ParamType::kNPC:\s*param.form = LookupTypedFormByToken<RE::TESNPC>') {
        throw "Condition Actor references and ActorBase records must stay distinct"
    }
    $conditionSave = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/Menu.Conditions.State.cpp") -Raw
    $conditionValueEditors = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/conditions/ValueEditors.cpp") -Raw
    $conditionClauseTable = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/Menu.Conditions.ClauseTable.cpp") -Raw
    $conditionFormTokens = Get-Content -LiteralPath (
        Join-Path $repository "src/conditions/FormTokens.cpp") -Raw
    if (-not $conditionClauseTable.Contains('ImGui::GetContentRegionAvail().x, paramIndex == 0)') -or
        -not $conditionValueEditors.Contains('a_preferEditorID && optionCache.SupportsFormTokens(a_type)') -or
        -not $conditionFormTokens.Contains('LookupFormToken(a_token, false)')) {
        throw "EditorID display must be Arg1/form-only and must not scan all game records"
    }
    if ($conditionValueEditors -notmatch 'case RE::SCRIPT_PARAM_TYPE::kChar:\s*case RE::SCRIPT_PARAM_TYPE::kVMScriptVar:\s*return ValueEditorKind::Text;' -or
        -not $conditionValueEditors.Contains('DrawTextClauseValueEditor(a_id, a_value, a_width)') -or
        $conditionLowering -match 'case ParamType::kChar:\s*case ParamType::kInt:' -or
        -not $conditionLowering.Contains('a_storage.StoreText(argument)') -or
        -not $conditionLowering.Contains('a_literal.parameterTypes[paramIndex] == ParamType::kVMScriptVar') -or
        -not $conditionLowering.Contains('std::shared_ptr<RE::TESCondition>(storage, &storage->condition)')) {
        throw "kChar requires text input and condition-owned stable BSFixedString objects, never integers or draft c_str pointers"
    }
    $conditionDraft = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/conditions/DraftValidation.cpp") -Raw
    foreach ($valuePath in @($conditionLowering, $conditionDraft, $conditionSave, $conditionValueEditors)) {
        if ($valuePath -match 'std::sto[ifd]\(' -or
            -not $valuePath.Contains('TryParseFloat(') -or
            -not $valuePath.Contains('TryParseInt(')) {
            throw "Condition editing, validation, saving and lowering must share strict scalar parsing"
        }
    }
    foreach ($valuePath in @($conditionLowering, $conditionDraft)) {
        if (-not $valuePath.Contains('ParseAxisArgument(') -or
            -not $valuePath.Contains('ParseActorValueArgument(')) {
            throw "Draft validation and lowering must validate axis and runtime ActorValue names"
        }
    }
    if (-not $conditionSave.Contains('conditions.validation.parameter_text') -or
        -not $conditionDraft.Contains('conditions.validation.parameter_text')) {
        throw "Both condition draft validation and saving must report malformed string arguments"
    }
    if (-not $conditionSave.Contains('ResolveConditionFormArgument') -or
        -not $conditionSave.Contains('conditions.validation.parameter_form')) {
        throw "Free-form condition input requires typed save-time validation"
    }
    $conditionSerialization = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/Menu.Conditions.Serialization.cpp") -Raw
    if (-not $conditionSerialization.Contains('clauseJson.value("arg2", std::string{})') -or
        -not $conditionSerialization.Contains('{"arg2", clause.arguments[1]}')) {
        throw "VM variable names must retain their string representation through save/load"
    }
    $conditionOptions = Get-Content -LiteralPath (
        Join-Path $repository "src/ui/ConditionParamOptionCache.cpp") -Raw
    if (-not $conditionOptions.Contains('return sfs::conditions::CollectCellForms()') -or
        -not $conditionOptions.Contains('a_type != ParamType::kActorValue')) {
        throw "CELL sources and ActorValue/form-token separation must be preserved"
    }
    foreach ($conditionLocale in @('en', 'kor', 'zh_cn')) {
        $conditionLocaleJson = Get-Content -LiteralPath (
            Join-Path $repository "data/Interface/SkyrimFittingSystem/locales/$conditionLocale.json") -Raw | ConvertFrom-Json
        if (-not $conditionLocaleJson.strings.'conditions.validation.parameter_form') {
            throw "Missing condition form-input error translation: $conditionLocale"
        }
        if (-not $conditionLocaleJson.strings.'conditions.validation.parameter_text') {
            throw "Missing condition string-input error translation: $conditionLocale"
        }
        if (-not $conditionLocaleJson.strings.'conditions.vm_variable_hint') {
            throw "Missing VM variable-name guidance: $conditionLocale"
        }
        if (-not $conditionLocaleJson.strings.'conditions.validation.parameter_choice') {
            throw "Missing invalid named value guidance: $conditionLocale"
        }
    }
    Write-Host "Condition form-input, actor-type, and localized validation checks passed"

    $catalogFilterSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Catalog.Filters.cpp") -Raw
    if (-not $catalogFilterSource.Contains("body_family::Matches") -or
        -not $catalogFilterSource.Contains("catalogBodyFamilyFilterEnabled_")) {
        throw "Equipment, Outfits, and Kits must retain the optional BodyFamily filter"
    }

    $workbenchFilterSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Workbench.Filters.cpp") -Raw
    if ($workbenchFilterSource.Contains("body_family::")) {
        throw "The registered-appearance workbench must remain outside BodyFamily filtering"
    }

    $settingsSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Settings.cpp") -Raw
    $criticalSettingKeys = @(
        "pauseGameWhileOpen",
        "smoothScroll",
        "menuCharacterSide",
        "addCrosshairNpcToActorList",
        "catalogBodyFamilyFilterEnabled",
        "specialEffectProtectedSlots",
        "shieldAppearanceSlotEnabled",
        "externalModStripLinkMode",
        "customExternalModStripLinkBaseMode",
        "customDirectStripLinkAutomaticBaseMode",
        "customStripLinkDisabledAppearanceSlots",
        "customDirectStripLinkMappings",
        "customDirectStripLinkOverrideSlots",
        "hideRealEquipmentWithFitting"
    )
    foreach ($settingKey in $criticalSettingKeys) {
        # Every persistent option must occur in both LoadUserSettings and
        # SaveUserSettings. A single occurrence catches accidental load-only
        # or save-only migrations before they can silently reset a feature.
        $keyCount = ([regex]::Matches(
                $settingsSource,
                [regex]::Escape('"' + $settingKey + '"'))).Count
        if ($keyCount -lt 2) {
            throw "Critical option '$settingKey' must remain load/save symmetric"
        }
    }

    foreach ($locale in @("en.json", "kor.json", "zh_cn.json")) {
        $localePath = Join-Path $repository (
            "data\Interface\SkyrimFittingSystem\locales\" + $locale)
        $localeJson = Get-Content -LiteralPath $localePath -Raw |
            ConvertFrom-Json
        if (-not $localeJson.strings.PSObject.Properties[
                "options.catalog_body_family_filter"] -or
            -not $localeJson.strings.PSObject.Properties[
                "options.catalog_body_family_filter.tooltip"] -or
            -not $localeJson.strings.PSObject.Properties[
                "dye.popup.multiply_note"] -or
            -not $localeJson.strings.PSObject.Properties[
                "help.conditions.4"]) {
            throw "Missing BodyFamily, Fitting Dye, or condition guidance translations in $locale"
        }
    }
    Write-Host "Optional catalog BodyFamily and workbench isolation checks passed"

    # BodyMorph state has one owner. Feature adapters may request an armor
    # refresh, but cannot directly publish, clear, or include the tracking
    # implementation. This keeps unrelated fixes (HT2, dye, camera, stripping,
    # DD, P+, and UI work) from silently taking ownership of live morph state.
    $morphOwnershipPatterns = @(
        "SetRegisteredAppearanceDisplayActive",
        "ForgetAllRegisteredAppearanceNodes"
    )
    $allowedMorphOwners = @(
        "src\native\ArmorSkinning.cpp",
        "src\native\RaceMenuBodyMorph.cpp",
        "src\native\RaceMenuBodyMorph.h",
        "src\Serialization.cpp",
        "src\main.cpp"
    )
    $sourceFiles = Get-ChildItem -LiteralPath (Join-Path $repository "src") `
        -Recurse -File -Include *.cpp,*.h
    foreach ($sourceFile in $sourceFiles) {
        # Windows PowerShell 5.1 runs on a .NET Framework version that does not
        # expose Path.GetRelativePath. Every enumerated file is already rooted
        # below $repository, so derive the relative name without depending on
        # that newer runtime API.
        $relative = $sourceFile.FullName.Substring($repository.Length).TrimStart(
            [char[]]"\/")
        $content = Get-Content -LiteralPath $sourceFile.FullName -Raw
        foreach ($pattern in $morphOwnershipPatterns) {
            if ($content.Contains($pattern) -and
                $allowedMorphOwners -notcontains $relative) {
                throw "BodyMorph ownership escaped into $relative via $pattern"
            }
        }
    }

    $armorSkinningSource = Get-Content -LiteralPath (
        Join-Path $repository "src\native\ArmorSkinning.cpp") -Raw
    $tokenPerformanceSource = Get-Content -LiteralPath (
        Join-Path $repository "src/features/virtual_tokens/VirtualWornTokens.cpp") -Raw
    $tokenRebuild = [regex]::Match($tokenPerformanceSource,
        '(?s)void UpdateVirtualWornTokenCache\(\).*?(?=void InitializeVirtualWornTokens\()').Value
    $bulkLookup = [regex]::Match($armorSkinningSource,
        '(?s)GetActiveFittingAppearancesBySlot\(.*?(?=RE::TESObjectARMO \*\s*GetActiveFittingArmorForSlot)').Value
    if (-not $tokenRebuild.Contains('GetActiveFittingAppearancesBySlot(actor, true)') -or
        $tokenRebuild.Contains('GetActiveFittingAppearanceForSlot(') -or
        ([regex]::Matches($bulkLookup, 'CollectActiveFittingArmors\(')).Count -ne 1 -or
        -not $bulkLookup.Contains('BuildFirstAppearanceSlotLookup(entries)')) {
        throw "Token rebuilds must evaluate each actor once, preserving all 32 first-match slot results"
    }
    $bodyKeywordQuery = [regex]::Match($armorSkinningSource,
        '(?s)GetDisplayedBodyKeywordState\(.*?(?=bool IsSFSOwnedRuntimeKeyword)').Value
    if ($bodyKeywordQuery -notmatch 'if \(!displaySet.active\)\s*\{\s*return std::nullopt;\s*\}\s*const auto snapshot = BuildFinalRenderedOutfitSnapshot' -or
        $bodyKeywordQuery.Contains('GetFinalRenderedOutfitSnapshot(a_actor)')) {
        throw "Unmanaged WornHasKeyword queries must retain vanilla results without collecting a final worn snapshot"
    }
    $displayBuilder = [regex]::Match($armorSkinningSource,
        '(?s)BuildDisplaySet\(RE::Actor \*a_actor,.*?(?=\[\[nodiscard\]\] std::unordered_set)').Value
    if (-not $displayBuilder.Contains('(a_equippedSnapshot ? *a_equippedSnapshot : localEquipped).Get()') -or
        $displayBuilder.Contains('CollectEquippedArmors(') -or
        -not $bodyKeywordQuery.Contains('BuildDisplaySet(a_actor, true, &equipped)') -or
        -not $bodyKeywordQuery.Contains('BuildFinalRenderedOutfitSnapshot(a_actor, displaySet, equipped)')) {
        throw "Final body queries must reuse one request-local worn snapshot, without changing runtime scan coverage"
    }
    $dyePerformanceSource = Get-Content -LiteralPath (
        Join-Path $repository "src/native/FittingDye.cpp") -Raw
    if (-not $dyePerformanceSource.Contains('!inspectTargets && g_activeRenderPasses.empty()') -or
        -not $dyePerformanceSource.Contains('rules::ShouldTrackRendererTintPass(') -or
        $dyePerformanceSource -notmatch 'if \(g_worldTintPreview\)\s*\{\s*const auto now = std::chrono::steady_clock::now\(\)' -or
        $dyePerformanceSource -notmatch 'if \(trackPass\)\s*\{\s*g_activeRenderPasses.push_back') {
        throw "Dye fast paths must retain nested-pass masking and avoid idle timer/record work"
    }
    Write-Host "City hot-path source wiring checks passed (not an in-game frame-time benchmark)"
    $synchronizeIndex = $armorSkinningSource.IndexOf(
        "helmet_toggle::SynchronizeActor(a_actor, false)")
    $publishIndex = $armorSkinningSource.IndexOf(
        "SetRegisteredAppearanceDisplayActive(`r`n      a_actor", $synchronizeIndex)
    if ($publishIndex -lt 0) {
        $publishIndex = $armorSkinningSource.IndexOf(
            "SetRegisteredAppearanceDisplayActive(`n      a_actor", $synchronizeIndex)
    }
    $backendPlanIndex = $armorSkinningSource.IndexOf(
        "refresh_rules::BuildPlan", $publishIndex)
    if ($synchronizeIndex -lt 0 -or $publishIndex -lt 0 -or
        $backendPlanIndex -lt 0 -or $publishIndex -ge $backendPlanIndex) {
        throw "RefreshArmorFor must publish BodyMorph activity before selecting or dispatching a DAVE/DAV/native backend"
    }

    $xmakeSource = Get-Content -LiteralPath (
        Join-Path $repository "xmake.lua") -Raw
    $renderedProviderSource = Get-Content -LiteralPath (
        Join-Path $repository "src/api/RenderedOutfitProvider.cpp") -Raw
    $productionTargetStart = $xmakeSource.IndexOf('target("SkyrimFittingSystem")')
    $testTargetsStart = $xmakeSource.IndexOf('target("IedConditionIntegrationTests")')
    if ($productionTargetStart -lt 0 -or $testTargetsStart -le $productionTargetStart) {
        throw "Cannot identify the production/test build boundary"
    }
    $productionTarget = $xmakeSource.Substring($productionTargetStart,
        $testTargetsStart - $productionTargetStart)
    foreach ($temporaryProbe in @('BCNGSFSTaskThreadProbe', 'task-thread-probe',
        'consumer-probe', 'read-live.ps1', 'sfs-live-20260913', 'ReadProcessMemory',
        'WriteProcessMemory', 'OpenProcess', 'SFS_RENDERED_OUTFIT_TEST')) {
        # The conditional test boundary in the provider is intentional. The
        # production target must never define it or link external audit probes.
        if ($productionTarget.Contains($temporaryProbe) -or
            ($temporaryProbe -ne 'SFS_RENDERED_OUTFIT_TEST' -and
             $renderedProviderSource.Contains($temporaryProbe))) {
            throw "Temporary analysis code entered a production API/build boundary: $temporaryProbe"
        }
    }
    if (-not $xmakeSource.Contains('set_config("skyrim_vr", false)')) {
        throw "The production build must remain SE/AE-only with the exact flat layout"
    }
    $virtualTokenSource = Join-Path $repository (
        "src\features\virtual_tokens\VirtualWornTokens.cpp")
    $deviousDevicesSource = Join-Path $repository (
        "src\features\devious_devices\DeviousDevicesIntegration.cpp")
    if (-not (Test-Path -LiteralPath $virtualTokenSource -PathType Leaf) -or
        -not (Test-Path -LiteralPath $deviousDevicesSource -PathType Leaf)) {
        throw "Virtual Tokens and Devious Devices must remain official production features"
    }
    foreach ($featureSource in @($virtualTokenSource, $deviousDevicesSource)) {
        $featureText = Get-Content -LiteralPath $featureSource -Raw
        if ($featureText.Contains('SFS_VIRTUAL_TOKENS') -or
            $featureText -match 'void\s+(InitializeVirtualWornTokens|InitializeDeviousDevicesHider)\([^)]*\)\s*\{\s*\}') {
            throw "Strip integration must be unconditional production code, never an optional empty stub"
        }
    }
    if ($xmakeSource.Contains('SFS_VIRTUAL_TOKENS')) {
        throw "Obsolete strip-integration build switches must not return"
    }
    if ($xmakeSource.Contains("SFS_VIRTUAL_TOKEN_POC") -or
        $xmakeSource.Contains("poc/VirtualWornTokenPoC") -or
        (Test-Path -LiteralPath (
            Join-Path $repository "src\poc\DeviousDevicesHiderPoC.cpp"))) {
        throw "Obsolete Virtual Token/DD PoC build identity must not return"
    }

    $workbenchTableSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Workbench.Table.cpp") -Raw
    if (-not $workbenchTableSource.Contains(
            "ApplyConditionalActionDropTransaction") -or
        $workbenchTableSource.Contains("clearMovedActionSource")) {
        throw "Condition action-card moves must remain one workbench transaction"
    }

    $workbenchModelSource = Get-Content -LiteralPath (
        Join-Path $repository "src\VariantWorkbench.cpp") -Raw
    $rawAutomationInvalidationCount = ([regex]::Matches(
            $workbenchModelSource,
            "sfs::virtual_tokens::InvalidateVirtualWornTokenAutomationForAppearance")).Count
    if ($rawAutomationInvalidationCount -ne 1 -or
        -not $workbenchModelSource.Contains(
            "InvalidateRemovedAppearanceAutomation(previousRows)")) {
        throw "Workbench edits must release strip/DD automation through the centralized retained-identity boundary"
    }
    $workbenchAcceptCount = ([regex]::Matches(
            $workbenchTableSource, "AcceptDragDropPayload")).Count
    $workbenchEarlyAcceptCount = ([regex]::Matches(
            $workbenchTableSource,
            "ImGuiDragDropFlags_AcceptBeforeDelivery")).Count
    $workbenchDeliveryCount = ([regex]::Matches(
            $workbenchTableSource, "payload->IsDelivery\(\)")).Count
    if ($workbenchAcceptCount -eq 0 -or
        $workbenchAcceptCount -ne $workbenchEarlyAcceptCount -or
        $workbenchAcceptCount -ne $workbenchDeliveryCount) {
        throw "Every condition workbench drop target must register before and commit on the first delivery"
    }

    $clauseTableSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Conditions.ClauseTable.cpp") -Raw
    $clauseAcceptCount = ([regex]::Matches(
            $clauseTableSource, "AcceptDragDropPayload")).Count
    $clauseEarlyAcceptCount = ([regex]::Matches(
            $clauseTableSource,
            "ImGuiDragDropFlags_AcceptBeforeDelivery")).Count
    $clauseDeliveryCount = ([regex]::Matches(
            $clauseTableSource, "payload->IsDelivery\(\)")).Count
    if (-not $clauseTableSource.Contains("ApplyMoveTransaction") -or
        -not $clauseTableSource.Contains("sourceFingerprint") -or
        $clauseAcceptCount -eq 0 -or
        $clauseAcceptCount -ne $clauseEarlyAcceptCount -or
        $clauseAcceptCount -ne $clauseDeliveryCount) {
        throw "Condition clause reordering must reject stale payloads atomically"
    }

    $armorHookSource = Get-Content -LiteralPath (
        Join-Path $repository "src\native\ArmorSkinningHooks.cpp") -Raw
    if (-not $armorHookSource.Contains("ResolveBranchChainOwner") -or
        -not $armorHookSource.Contains("SetPassthroughVisitWornItemsChainTarget")) {
        throw "Custom-skin compatibility must preserve unknown concrete visitors and SFS attachments"
    }
    $iedConditionSource = Get-Content -LiteralPath (
        Join-Path $repository "src/native/IedConditionIntegration.cpp") -Raw
    $iedQueueInvalidation = [regex]::Match($armorSkinningSource,
        '(?s)void InvalidateQueuedArmorRefreshes\(\)\s*\{.*?\n\}').Value
    if (-not $iedConditionSource.Contains('SetDecisionObserver(&QueueIedEvaluation)') -or
        $armorSkinningSource -notmatch 'void QueueIedEvaluation\(const std::uint32_t actorFormID\)\s*\{\s*QueueIedEvaluateID\(actorFormID\);' -or
        -not $iedQueueInvalidation.Contains('ClearQueuedIedEvaluations()') -or
        $iedConditionSource.Contains('VisitWornItems') -or
        $iedConditionSource.Contains('RefreshArmorFor(')) {
        throw "IED display conditions must reuse the load-canceled actor queue, never reenter SFS/custom-skin refresh"
    }

    $footwearApiSource = Get-Content -LiteralPath (
        Join-Path $repository "src\api\SkyrimFittingSystemAPI.cpp") -Raw
    $footprintsPatchSource = Get-Content -LiteralPath (
        Join-Path $repository "compat\DynamicFootprintsSfsPatch\src\main.cpp") -Raw
    $consumerApiSource = Get-Content -LiteralPath (
        Join-Path $repository "extras\SkyrimFittingSystemAPI.h") -Raw
    if (-not $footwearApiSource.Contains(
            "SkyrimFittingSystem_TryGetDisplayedFootwearFormID") -or
        -not $footwearApiSource.Contains("ResolveDisplayedFootwear") -or
        -not $footprintsPatchSource.Contains(
            "SkyrimFittingSystem_TryGetDisplayedFootwearFormID") -or
        -not $footprintsPatchSource.Contains("finalFormID != 0") -or
        -not $consumerApiSource.Contains(
            "SkyrimFittingSystem_TryGetDisplayedFootwearFormID_t") -or
        -not $consumerApiSource.Contains(
            "SkyrimFittingSystem_TryGetDisplayedFootwearFormID_Name")) {
        throw "Dynamic Footprints must preserve explicit final-render barefoot state"
    }
    $footprintsPluginHeader = Get-Content -LiteralPath (
        Join-Path $repository "compat\DynamicFootprintsSfsPatch\src\Plugin.h") -Raw
    $footprintsReadme = Get-Content -LiteralPath (
        Join-Path $repository "compat\DynamicFootprintsSfsPatch\README.txt") -Raw
    $footprintsPackageScriptPath = Join-Path $repository `
        "scripts\package-dynamic-footprints-patch.ps1"
    if (-not $footprintsPluginHeader.Contains(
            "REL::Version VERSION{1, 6, 0, 0}") -or
        -not $footprintsPluginHeader.Contains(
            'VERSION_STRING{"1.6.0"}') -or
        -not $footprintsReadme.StartsWith(
            "SFS - Dynamic Footprints Compatibility Patch v1.6.0") -or
        -not (Test-Path -LiteralPath $footprintsPackageScriptPath -PathType Leaf)) {
        throw "Dynamic Footprints source, README, and package version must remain aligned"
    }

    $mainSource = Get-Content -LiteralPath (
        Join-Path $repository "src\main.cpp") -Raw
    $menuSettingsSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Settings.cpp") -Raw
    $installPath = Join-Path $repository "INSTALL.txt"
    $packageSource = Get-Content -LiteralPath (
        Join-Path $repository "scripts\package-release.ps1") -Raw
    $skseInitIndex = $mainSource.IndexOf("SKSE::Init(a_skse)")
    $settingsPrepareIndex = $mainSource.IndexOf(
        "PrepareUserSettingsStorage()", $skseInitIndex)
    if ($skseInitIndex -lt 0 -or $settingsPrepareIndex -le $skseInitIndex -or
        -not $menuSettingsSource.Contains(
            "void Menu::PrepareUserSettingsStorage()") -or
        -not $menuSettingsSource.Contains(
            'logger::info("SFS user settings path: {}"') -or
        -not (Test-Path -LiteralPath $installPath -PathType Leaf)) {
        throw "First-install diagnostics must create and log settings storage immediately after SKSE initializes"
    }
    $installSource = Get-Content -LiteralPath $installPath -Raw
    if (-not $installSource.Contains("skse64_loader.exe") -or
        -not $installSource.Contains("Escape -> Controls") -or
        -not $installSource.Contains("Skyrim Fitting System.log") -or
        ([regex]::Matches($packageSource,
            "'INSTALL.txt'")).Count -lt 2) {
        throw "The runtime/source packages must retain explicit MO2/Vortex F6 first-install guidance"
    }

    $serializationSource = Get-Content -LiteralPath (
        Join-Path $repository "src\Serialization.cpp") -Raw
    $appearanceLifecycle = Get-Content -LiteralPath (
        Join-Path $repository "src/native/AppearanceResourceLifecycle.cpp") -Raw
    $dyeLifecycle = Get-Content -LiteralPath (
        Join-Path $repository "src/native/FittingDye.cpp") -Raw
    if (-not $mainSource.Contains('sfs::native::appearance_resources::RegisterEvents()') -or
        -not $appearanceLifecycle.Contains('racemenu::ReleaseActorSceneResources(event->formID, false)') -or
        -not $appearanceLifecycle.Contains('racemenu::ReleaseActorSceneResources(event->formID, true)') -or
        -not $appearanceLifecycle.Contains('dye::ReleaseActorSceneResources(event->formID)') -or
        -not $appearanceLifecycle.Contains('dye::RestoreActorSceneResources(event->formID)') -or
        $appearanceLifecycle -match 'RefreshArmorFor|QueueArmorRefresh|EquipObject|ForgetAllRegisteredAppearanceNodes|ClearWorldTint\(') {
        throw "Scene resource lifecycle must stay event-driven, actor-local, and independent of renderer refresh/visibility"
    }
    if (-not $dyeLifecycle.Contains('status, a_ticket)') -or
        -not $dyeLifecycle.Contains('WorldTintBuild build(a_actorFormID, a_restoreTicket)') -or
        ([regex]::Matches($dyeLifecycle, 'if \(!build.CurrentLocked\(\)\)')).Count -ne 2 -or
        -not $dyeLifecycle.Contains('.target = {.actorFormID = a_actorFormID')) {
        throw "Dye restore, direct builds and previews must retain actor-local cancellation fences"
    }
    $prepareStart = $serializationSource.IndexOf(
        "void PrepareForLoadTransition()")
    $saveStart = $serializationSource.IndexOf(
        "void SaveCallback", $prepareStart)
    $loadStart = $serializationSource.IndexOf("void LoadCallback")
    $loadPrepare = $serializationSource.IndexOf(
        "PrepareForLoadTransition()", $loadStart)
    $loadDeserialize = $serializationSource.IndexOf(
        ".Deserialize(", $loadStart)
    if ($prepareStart -lt 0 -or $saveStart -le $prepareStart -or
        $loadStart -lt 0 -or $loadPrepare -le $loadStart -or
        $loadDeserialize -le $loadPrepare -or
        -not $mainSource.Contains(
            "case SKSE::MessagingInterface::kPreLoadGame:") -or
        -not $mainSource.Contains(
            "sfs::serialization::PrepareForLoadTransition();")) {
        throw "Save loading must enter one idempotent lifecycle boundary before deserialization"
    }
    $prepareBody = $serializationSource.Substring(
        $prepareStart, $saveStart - $prepareStart)
    foreach ($requiredReset in @(
        "SetGameDataLoaded(false)",
        "InvalidateQueuedArmorRefreshes()",
        "CancelQueuedRefreshes()",
        "CancelSOSStorageSync()",
        "ForgetAllRegisteredAppearanceNodes()",
        "ClearWorldTint()",
        "RevertSavedWorldTints()",
        "ResetVirtualWornTokenRuntimeState()",
        "external_equipment::ClearRuntimeState()",
        "helmet_toggle::ResetRuntimeState()",
        "ClearAllFittingSlotStates()",
        "dave::ForgetHiddenRealEquipmentState()"
    )) {
        if (-not $prepareBody.Contains($requiredReset)) {
            throw "Save-load lifecycle boundary lost required reset: $requiredReset"
        }
    }

    $dyeUiSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Workbench.Dye.cpp") -Raw
    if (-not $dyeUiSource.Contains('"dye.popup.multiply_note"')) {
        throw "Fitting Dye UI must explain that white is a neutral multiply tint"
    }
    $browserUiSource = Get-Content -LiteralPath (
        Join-Path $repository "src\ui\Menu.Browser.cpp") -Raw
    if (-not $browserUiSource.Contains('"help.conditions.4"')) {
        throw "Conditions UI must explain independent truth and non-priority ordering"
    }

    $wetPatchRoot = Join-Path $repository "compat\WetFunctionReduxSfsPatch"
    $wetRuntimeFiles = @(Get-ChildItem -LiteralPath (
        Join-Path $wetPatchRoot "scripts") -File -Recurse)
    $wetSource = Get-Content -LiteralPath (
        Join-Path $wetPatchRoot "source\scripts\WetFunctionEffect.psc") -Raw
    $wetReadme = Get-Content -LiteralPath (
        Join-Path $wetPatchRoot `
            "README - SFS Wet Function Redux Compatibility Patch.txt") -Raw
    $wetPackageScript = Get-Content -LiteralPath (
        Join-Path $repository "scripts\package-wet-function-patch.ps1") -Raw
    $wetCompiledText = [System.Text.Encoding]::UTF8.GetString(
        [System.IO.File]::ReadAllBytes($wetRuntimeFiles[0].FullName))
    if ($wetRuntimeFiles.Count -ne 1 -or
        $wetRuntimeFiles[0].Name -cne "WetFunctionEffect.pex" -or
        -not $wetSource.Contains("IsRealEquipmentHiddenForActorSlots") -or
        -not $wetSource.Contains("GetDisplayedFittingSlotMask") -or
        -not $wetReadme.Contains("Do NOT install or overwrite WetFunctionMCM.pex") -or
        -not $wetPackageScript.Contains('$expectedEntries') -or
        -not $wetPackageScript.Contains("WetFunctionMCM|settings") -or
        -not $wetPackageScript.Contains("WetFunctionEffect.pex") -or
        -not $wetCompiledText.Contains("SFSShouldOperateSlot") -or
        -not $wetCompiledText.Contains("IsRealEquipmentHiddenForActorSlots") -or
        -not $wetCompiledText.Contains("GetDisplayedFittingSlotMask")) {
        throw "Wet Function patch must remain a final-display-only, one-script, save-safe override"
    }

    $dffmaRoot = Join-Path $repository `
        "compat\DynamicFeminineFemaleModestyAnimationsSfsPatch"
    $dffmaModulePath = Join-Path $dffmaRoot "fomod\ModuleConfig.xml"
    $dffmaInfoPath = Join-Path $dffmaRoot "fomod\info.xml"
    $dffmaFlatModulePath = Join-Path $dffmaRoot "fomod-ModuleConfig.xml"
    $dffmaFlatInfoPath = Join-Path $dffmaRoot "fomod-info.xml"
    foreach ($requiredDffmaPath in @(
        $dffmaModulePath, $dffmaInfoPath, $dffmaFlatModulePath,
        $dffmaFlatInfoPath
    )) {
        if (-not (Test-Path -LiteralPath $requiredDffmaPath -PathType Leaf)) {
            throw "DFFMA FOMOD metadata is missing: $requiredDffmaPath"
        }
    }
    if ((Get-FileHash -LiteralPath $dffmaModulePath -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $dffmaFlatModulePath -Algorithm SHA256).Hash -or
        (Get-FileHash -LiteralPath $dffmaInfoPath -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $dffmaFlatInfoPath -Algorithm SHA256).Hash) {
        throw "DFFMA canonical and source-package FOMOD metadata must remain identical"
    }
    try {
        [xml]$dffmaModule = Get-Content -LiteralPath $dffmaModulePath -Raw
        [xml]$dffmaInfo = Get-Content -LiteralPath $dffmaInfoPath -Raw
    } catch {
        throw "DFFMA FOMOD XML is not well formed: $($_.Exception.Message)"
    }
    if ([string]$dffmaInfo.fomod.Version -ne "1.4.4.1") {
        throw "DFFMA corrected FOMOD package version must remain 1.4.4.1"
    }
    $dffmaPlugins = @($dffmaModule.SelectNodes(
        "/config/installSteps/installStep/optionalFileGroups/group/plugins/plugin"))
    if ($dffmaPlugins.Count -ne 16) {
        throw "DFFMA FOMOD must expose all 16 player/NPC installation choices"
    }
    foreach ($plugin in $dffmaPlugins) {
        $elements = @($plugin.ChildNodes | Where-Object {
            $_.NodeType -eq [System.Xml.XmlNodeType]::Element
        })
        if ($elements.Count -lt 2 -or $elements[0].Name -ne "description" -or
            [string]::IsNullOrWhiteSpace($elements[0].InnerText) -or
            $elements[$elements.Count - 1].Name -ne "typeDescriptor") {
            throw "DFFMA choice '$($plugin.name)' violates the FOMOD 5 description/.../typeDescriptor order"
        }
    }
    foreach ($folder in @($dffmaModule.SelectNodes("//folder"))) {
        $relativeSource = ([string]$folder.source).Replace(
            [char]'\', [System.IO.Path]::DirectorySeparatorChar)
        $sourcePath = Join-Path $dffmaRoot $relativeSource
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Container) -or
            @(Get-ChildItem -LiteralPath $sourcePath -File -Recurse).Count -eq 0) {
            throw "DFFMA FOMOD references a missing or empty source folder: $($folder.source)"
        }
    }
    if (-not (Test-Path -LiteralPath (
            Join-Path $repository "scripts\package-dffma-patch.ps1") -PathType Leaf)) {
        throw "The validated DFFMA package builder must remain available"
    }

    Write-Host "BodyMorph, condition DnD, IED, final footwear, strip-link, save lifecycle, dye UX, first-install, compatibility packages, and DFFMA FOMOD checks passed"
} finally {
    Pop-Location
}
