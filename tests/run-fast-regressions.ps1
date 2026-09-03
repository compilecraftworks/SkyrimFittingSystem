param(
    [string]$Xmake = "xmake"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $repository "VERSION") -Raw).Trim()
$targets = @(
    "RuntimeLayoutTests",
    "KitGeneratorLogicTests",
    "BodyFamilyLogicTests",
    "ConditionCnfLogicTests",
    "FittingDyeRulesTests",
    "KitListNavigationTests",
    "CoreBehaviorRegressionTests"
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
    if (-not $settingsSource.Contains('"catalogBodyFamilyFilterEnabled"')) {
        throw "The optional BodyFamily catalog filter must persist in user settings"
    }

    foreach ($locale in @("en.json", "kor.json", "zh_cn.json")) {
        $localePath = Join-Path $repository (
            "data\Interface\SkyrimFittingSystem\locales\" + $locale)
        $localeJson = Get-Content -LiteralPath $localePath -Raw |
            ConvertFrom-Json
        if (-not $localeJson.strings.PSObject.Properties[
                "options.catalog_body_family_filter"] -or
            -not $localeJson.strings.PSObject.Properties[
                "options.catalog_body_family_filter.tooltip"]) {
            throw "Missing BodyFamily filter translations in $locale"
        }
    }
    Write-Host "Optional catalog BodyFamily and workbench isolation checks passed"
} finally {
    Pop-Location
}
