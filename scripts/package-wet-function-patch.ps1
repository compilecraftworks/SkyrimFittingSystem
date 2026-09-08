param(
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repository = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory '..'))
$patchRoot = Join-Path $repository 'compat\WetFunctionReduxSfsPatch'
$effectScript = Join-Path $patchRoot 'scripts\WetFunctionEffect.pex'
$readme = Join-Path $patchRoot `
    'README - SFS Wet Function Redux Compatibility Patch.txt'
$packageVersion = '1.2.0'

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repository (
        "Release\SFS - Wet Function Redux Visual Effect Patch v$packageVersion.zip")
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

foreach ($requiredFile in @($effectScript, $readme)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required Wet Function patch file is missing: $requiredFile"
    }
}

# This compatibility package is intentionally a one-script override. Wet
# Function's own MCM owns its RaceMenu checks and serialized settings.
$runtimePatchFiles = @(Get-ChildItem -LiteralPath (
    Join-Path $patchRoot 'scripts') -File -Recurse)
if ($runtimePatchFiles.Count -ne 1 -or
    $runtimePatchFiles[0].Name -cne 'WetFunctionEffect.pex') {
    throw 'Wet Function runtime patch must contain only WetFunctionEffect.pex'
}
$compiledEffectText = [System.Text.Encoding]::UTF8.GetString(
    [System.IO.File]::ReadAllBytes($effectScript))
foreach ($requiredCompiledSymbol in @(
    'SFSShouldOperateSlot',
    'IsRealEquipmentHiddenForActorSlots',
    'GetDisplayedFittingSlotMask'
)) {
    if (-not $compiledEffectText.Contains($requiredCompiledSymbol)) {
        throw "WetFunctionEffect.pex is stale or missing $requiredCompiledSymbol"
    }
}

$stageRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $repository "tmp\wet-function-package-stage-v$packageVersion"))
$repositoryPrefix = $repository.TrimEnd('\') + '\'
if (-not $stageRoot.StartsWith(
        $repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a staging path outside the repository: $stageRoot"
}
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
$stageScripts = Join-Path $stageRoot 'scripts'
[System.IO.Directory]::CreateDirectory($stageScripts) | Out-Null
Copy-Item -LiteralPath $effectScript -Destination (
    Join-Path $stageScripts 'WetFunctionEffect.pex') -Force
Copy-Item -LiteralPath $readme -Destination (Join-Path $stageRoot 'README.txt') `
    -Force

$outputDirectory = Split-Path -Parent $OutputPath
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $stageRoot, $OutputPath,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)

$archive = [System.IO.Compression.ZipFile]::OpenRead($OutputPath)
try {
    $entryNames = @($archive.Entries | ForEach-Object {
        $_.FullName.Replace([char]'\', [char]'/')
    })
    $expectedEntries = @('README.txt', 'scripts/WetFunctionEffect.pex')
    if ($entryNames.Count -ne $expectedEntries.Count -or
        @($expectedEntries | Where-Object { $entryNames -notcontains $_ }).Count -ne 0 -or
        @($entryNames | Where-Object {
            $_ -match '(?i)WetFunctionMCM|settings|\.esp$|\.esl$|\.dll$'
        }).Count -ne 0) {
        throw 'Packaged Wet Function patch contains an unsafe or unexpected file'
    }
} finally {
    $archive.Dispose()
}

$hash = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash
$hashPath = [System.IO.Path]::ChangeExtension($OutputPath, '.sha256.txt')
[System.IO.File]::WriteAllText(
    $hashPath,
    "$hash *$([System.IO.Path]::GetFileName($OutputPath))`r`n",
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Wet Function patch: $OutputPath"
Write-Host "SHA-256:          $hash"
