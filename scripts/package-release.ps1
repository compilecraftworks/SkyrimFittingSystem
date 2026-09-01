param(
    [ValidateSet('release', 'releasedbg')]
    [string]$Mode = 'releasedbg',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory '..'))
$version = ([System.IO.File]::ReadAllText((Join-Path $repoRoot 'VERSION'))).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION must contain major.minor.patch, got '$version'."
}

$xmake = Join-Path $env:USERPROFILE '.codex\external-tools\xmake\3.1.0\xmake\xmake.exe'
if (-not (Test-Path -LiteralPath $xmake -PathType Leaf)) {
    throw "Pinned xmake v3.1.0 was not found at $xmake"
}

$buildDirectory = Join-Path $repoRoot "build\v$version\windows\x64\$Mode"
$dllPath = Join-Path $buildDirectory 'SFSCore.dll'

if (-not $SkipBuild) {
    $previousBuildVersion = $env:SFS_BUILD_VERSION
    $previousBuildVersionString = $env:SFS_BUILD_VERSION_STRING
    try {
        $env:SFS_BUILD_VERSION = $version
        $env:SFS_BUILD_VERSION_STRING = $version
        & $xmake f -P $repoRoot -y -m $Mode --skyrim_se=y --skyrim_ae=y --skyrim_vr=n
        if ($LASTEXITCODE -ne 0) { throw "xmake configure failed with exit code $LASTEXITCODE" }
        & $xmake -P $repoRoot -y SkyrimFittingSystem
        if ($LASTEXITCODE -ne 0) { throw "xmake build failed with exit code $LASTEXITCODE" }
    }
    finally {
        $env:SFS_BUILD_VERSION = $previousBuildVersion
        $env:SFS_BUILD_VERSION_STRING = $previousBuildVersionString
    }
}

if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
    throw "The release DLL was not found at $dllPath"
}
$versionInfo = (Get-Item -LiteralPath $dllPath).VersionInfo
$expectedDllVersion = "$version.0"
if ($versionInfo.FileVersion -ne $expectedDllVersion -or
    $versionInfo.ProductVersion -ne $expectedDllVersion) {
    throw "DLL version mismatch: expected $expectedDllVersion, got FileVersion=$($versionInfo.FileVersion), ProductVersion=$($versionInfo.ProductVersion)"
}

$stageRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "tmp\release-stage-v$version"))
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $stageRoot.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a staging directory outside the repository: $stageRoot"
}
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
$runtimeStage = Join-Path $stageRoot 'runtime'
$sourceStage = Join-Path $stageRoot 'source'
[System.IO.Directory]::CreateDirectory($runtimeStage) | Out-Null
[System.IO.Directory]::CreateDirectory($sourceStage) | Out-Null

function Copy-RequiredFile {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required release file is missing: $Source"
    }
    $destinationDirectory = Split-Path -Parent $Destination
    if ($destinationDirectory) {
        [System.IO.Directory]::CreateDirectory($destinationDirectory) | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-RequiredDirectory {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required source directory is missing: $Source"
    }
    $destinationParent = Split-Path -Parent $Destination
    [System.IO.Directory]::CreateDirectory($destinationParent) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

# Flat MO2-installable runtime root. No Data wrapper and no optional patch.
Copy-RequiredFile (Join-Path $repoRoot 'LICENSE') (Join-Path $runtimeStage 'LICENSE.txt')
Copy-RequiredFile (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') (Join-Path $runtimeStage 'THIRD_PARTY_NOTICES.txt')
Copy-RequiredFile (Join-Path $repoRoot 'data\SkyrimFittingSystem-VirtualTokens.esl') (Join-Path $runtimeStage 'SkyrimFittingSystem-VirtualTokens.esl')
Copy-RequiredDirectory (Join-Path $repoRoot 'data\Interface') (Join-Path $runtimeStage 'Interface')
Copy-RequiredDirectory (Join-Path $repoRoot 'data\Scripts') (Join-Path $runtimeStage 'Scripts')
Copy-RequiredFile $dllPath (Join-Path $runtimeStage 'SKSE\Plugins\SFSCore.dll')

$runtimePdbFiles = Get-ChildItem -LiteralPath $runtimeStage -Recurse -File -Filter '*.pdb'
if ($runtimePdbFiles) {
    throw "Debug symbols entered the runtime stage: $($runtimePdbFiles.FullName -join ', ')"
}

# Minimal complete corresponding source. Build products, private experiments,
# historical replacement patches, media, package archives, and workspace data
# are deliberately excluded.
$rootSourceFiles = @(
    '.clang-format', '.clang-tidy', '.gitattributes', '.gitignore',
    'DEPENDENCIES.md', 'LICENSE', 'NuGet.Config', 'README.md',
    'RELEASE_SOURCE_NOTICE.txt', 'SOURCE_PACKAGE_README.txt',
    'THIRD_PARTY_NOTICES.md', 'VERSION'
)
foreach ($relativePath in $rootSourceFiles) {
    Copy-RequiredFile (Join-Path $repoRoot $relativePath) (Join-Path $sourceStage $relativePath)
}
Copy-RequiredFile (Join-Path $repoRoot 'xmake.lua') (Join-Path $sourceStage 'xmake.lua.txt')
Copy-RequiredFile (Join-Path $repoRoot 'xmake-requires.lock') (Join-Path $sourceStage 'xmake-requires.lock.txt')

foreach ($relativeDirectory in @('src', 'tests', 'scripts', 'extras')) {
    Copy-RequiredDirectory (Join-Path $repoRoot $relativeDirectory) (Join-Path $sourceStage $relativeDirectory)
}

$sourceDocs = @(
    'Build-Deploy-Release.md', 'EXTERNAL-MOD-STRIP-LINK-MODES-KO.md',
    'OpenAnimationReplacer-Conditions.md', 'SkyrimFittingSystem-Menu-API.md',
    'RELEASE-NOTES-v1.5.0.md', 'RELEASE-NOTES-v1.5.0-ko.md',
    'RELEASE-NOTES-v1.5.1.md', 'RELEASE-NOTES-v1.5.1-ko.md',
    'RELEASE-NOTES-v1.5.2.md', 'RELEASE-NOTES-v1.5.2-ko.md'
)
foreach ($name in $sourceDocs) {
    Copy-RequiredFile (Join-Path $repoRoot "docs\$name") (Join-Path $sourceStage "docs\$name")
}

foreach ($relativeDirectory in @(
    'compat\DynamicFootprintsSfsPatch\source',
    'compat\DynamicFootprintsSfsPatch\src',
    'compat\WetFunctionReduxSfsPatch\headers',
    'compat\WetFunctionReduxSfsPatch\source',
    'third_party\CommonLibSSE-NG'
)) {
    Copy-RequiredDirectory (Join-Path $repoRoot $relativeDirectory) (Join-Path $sourceStage $relativeDirectory)
}
Copy-RequiredFile (Join-Path $repoRoot 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\README.txt') (Join-Path $sourceStage 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\README.txt')
$dffmaModuleConfig = Join-Path $repoRoot 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod-ModuleConfig.xml'
$dffmaInfo = Join-Path $repoRoot 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod-info.xml'
if (-not (Test-Path -LiteralPath $dffmaModuleConfig -PathType Leaf)) {
    $dffmaModuleConfig = Join-Path $repoRoot 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod\ModuleConfig.xml'
}
if (-not (Test-Path -LiteralPath $dffmaInfo -PathType Leaf)) {
    $dffmaInfo = Join-Path $repoRoot 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod\info.xml'
}
Copy-RequiredFile $dffmaModuleConfig (Join-Path $sourceStage 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod-ModuleConfig.xml')
Copy-RequiredFile $dffmaInfo (Join-Path $sourceStage 'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch\fomod-info.xml')
Copy-RequiredFile (Join-Path $repoRoot 'compat\DynamicFootprintsSfsPatch\README.txt') (Join-Path $sourceStage 'compat\DynamicFootprintsSfsPatch\README.txt')
Copy-RequiredFile (Join-Path $repoRoot 'compat\WetFunctionReduxSfsPatch\README - SFS Wet Function Redux Compatibility Patch.txt') (Join-Path $sourceStage 'compat\WetFunctionReduxSfsPatch\README - SFS Wet Function Redux Compatibility Patch.txt')

foreach ($relativePath in @(
    'data\Scripts\Source\SkyrimFittingSystemNative.psc',
    'data\Interface\SkyrimFittingSystem\locales\en.json',
    'data\Interface\SkyrimFittingSystem\locales\kor.json',
    'data\Interface\SkyrimFittingSystem\locales\zh_cn.json',
    'data\Interface\SkyrimFittingSystem\themes\default.json',
    'data\Interface\SkyrimFittingSystem\user\kits\README.txt',
    'tools\CreateVirtualTokenPlugin\CreateVirtualTokenPlugin.csproj',
    'tools\CreateVirtualTokenPlugin\Program.cs'
)) {
    Copy-RequiredFile (Join-Path $repoRoot $relativePath) (Join-Path $sourceStage $relativePath)
}

$imguiFiles = @(
    'imconfig.h', 'imgui.cpp', 'imgui.h', 'imgui_draw.cpp',
    'imgui_internal.h', 'imgui_tables.cpp', 'imgui_widgets.cpp',
    'imstb_rectpack.h', 'imstb_textedit.h', 'imstb_truetype.h', 'LICENSE.txt',
    'backends\imgui_impl_dx11.cpp', 'backends\imgui_impl_dx11.h',
    'backends\imgui_impl_win32.cpp', 'backends\imgui_impl_win32.h'
)
foreach ($relativePath in $imguiFiles) {
    Copy-RequiredFile (Join-Path $repoRoot "lib\imgui\$relativePath") (Join-Path $sourceStage "lib\imgui\$relativePath")
}

$forbiddenSourceFiles = Get-ChildItem -LiteralPath $sourceStage -Recurse -File | Where-Object {
    $_.Extension -in @('.dll', '.pdb', '.pex', '.esl', '.zip', '.7z')
}
if ($forbiddenSourceFiles) {
    throw "Compiled or nested archive files entered the source stage: $($forbiddenSourceFiles.FullName -join ', ')"
}

$releaseDirectory = Join-Path $repoRoot 'Release'
$sourcesDirectory = Join-Path $repoRoot 'Sources'
$distDirectory = Join-Path $repoRoot 'dist'
foreach ($directory in @($releaseDirectory, $sourcesDirectory, $distDirectory)) {
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
}
$runtimeZip = Join-Path $releaseDirectory "Skyrim Fitting System v$version SE-AE.zip"
$sourceZip = Join-Path $sourcesDirectory "Skyrim Fitting System v$version Source.zip"
foreach ($archive in @($runtimeZip, $sourceZip)) {
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
}

[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $runtimeStage, $runtimeZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $sourceStage, $sourceZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

$distRuntimeZip = Join-Path $distDirectory (Split-Path -Leaf $runtimeZip)
$distSourceZip = Join-Path $distDirectory (Split-Path -Leaf $sourceZip)
Copy-Item -LiteralPath $runtimeZip -Destination $distRuntimeZip -Force
Copy-Item -LiteralPath $sourceZip -Destination $distSourceZip -Force

$hashLines = @()
foreach ($archive in @($runtimeZip, $sourceZip)) {
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    $hashLines += "$hash *$(Split-Path -Leaf $archive)"
}
$hashPath = Join-Path $releaseDirectory "SHA256SUMS-v$version.txt"
[System.IO.File]::WriteAllLines($hashPath, $hashLines, [System.Text.UTF8Encoding]::new($false))

Remove-Item -LiteralPath $stageRoot -Recurse -Force

Write-Output "Runtime: $runtimeZip"
Write-Output "Source:  $sourceZip"
Write-Output "Hashes:  $hashPath"
