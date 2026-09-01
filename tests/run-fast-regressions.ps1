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
} finally {
    Pop-Location
}
