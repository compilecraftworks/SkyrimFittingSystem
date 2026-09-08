param(
    [string]$Mode = 'releasedbg',
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repository = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory '..'))
$packageVersion = (Get-Content -LiteralPath (
    Join-Path $repository 'VERSION') -Raw).Trim()
$patchRoot = Join-Path $repository 'compat\DynamicFootprintsSfsPatch'
$binary = Join-Path $repository (
    "build\v$packageVersion\compat\dynamic-footprints\windows\x64\$Mode\" +
    'SFS_DynamicFootprintsPatch.dll')
$readme = Join-Path $patchRoot 'README.txt'
$sourceFiles = @(
    (Join-Path $patchRoot 'src\main.cpp'),
    (Join-Path $patchRoot 'src\Plugin.h'),
    (Join-Path $patchRoot 'source\xmake-target.patch')
)

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repository (
        "Release\SFS - Dynamic Footprints Compatibility Patch v$packageVersion.zip")
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

foreach ($requiredFile in @($binary, $readme) + $sourceFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required Dynamic Footprints patch file is missing: $requiredFile"
    }
}

$pluginHeader = Get-Content -LiteralPath $sourceFiles[1] -Raw
$readmeText = Get-Content -LiteralPath $readme -Raw
if (-not $pluginHeader.Contains(
        "REL::Version VERSION{$($packageVersion.Replace('.', ', ')), 0}") -or
    -not $pluginHeader.Contains(
        "VERSION_STRING{`"$packageVersion`"}") -or
    -not $readmeText.StartsWith(
        "SFS - Dynamic Footprints Compatibility Patch v$packageVersion")) {
    throw 'Dynamic Footprints README, source, and release versions differ'
}
$binaryInfo = (Get-Item -LiteralPath $binary).VersionInfo
if ($binaryInfo.FileVersion -ne "$packageVersion.0" -or
    $binaryInfo.ProductVersion -ne "$packageVersion.0") {
    throw "Dynamic Footprints binary version is not $packageVersion.0"
}

$stageRoot = [System.IO.Path]::GetFullPath((Join-Path $repository (
    "tmp\dynamic-footprints-package-stage-v$packageVersion")))
$repositoryPrefix = $repository.TrimEnd('\') + '\'
if (-not $stageRoot.StartsWith(
        $repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a staging path outside the repository: $stageRoot"
}
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
$pluginStage = Join-Path $stageRoot 'SKSE\Plugins'
$sourceStage = Join-Path $stageRoot 'source'
[System.IO.Directory]::CreateDirectory($pluginStage) | Out-Null
[System.IO.Directory]::CreateDirectory($sourceStage) | Out-Null
Copy-Item -LiteralPath $binary -Destination (
    Join-Path $pluginStage 'SFS_DynamicFootprintsPatch.dll') -Force
Copy-Item -LiteralPath $readme -Destination (Join-Path $stageRoot 'README.txt') `
    -Force
Copy-Item -LiteralPath $sourceFiles[0] -Destination (
    Join-Path $sourceStage 'main.cpp') -Force
Copy-Item -LiteralPath $sourceFiles[1] -Destination (
    Join-Path $sourceStage 'Plugin.h') -Force
Copy-Item -LiteralPath $sourceFiles[2] -Destination (
    Join-Path $sourceStage 'xmake-target.patch') -Force

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
    $expectedEntries = @(
        'README.txt',
        'SKSE/Plugins/SFS_DynamicFootprintsPatch.dll',
        'source/main.cpp',
        'source/Plugin.h',
        'source/xmake-target.patch'
    )
    if ($entryNames.Count -ne $expectedEntries.Count -or
        @($expectedEntries | Where-Object {
            $entryNames -notcontains $_
        }).Count -ne 0) {
        throw 'Packaged Dynamic Footprints archive has an unexpected layout'
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

Write-Host "Dynamic Footprints patch: $OutputPath"
Write-Host "SHA-256:                $hash"
