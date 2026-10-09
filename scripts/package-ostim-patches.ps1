$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = (Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim()
$patchRoot = Join-Path $repo 'compat/OStimSfsPatch'
$patchVersion = (Get-Content -LiteralPath (Join-Path $patchRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$' -or [version]$version -lt [version]'1.7.6' -or
    $patchVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid official core/patch version' }
$dll = Join-Path $repo "build/v$version/windows/x64/releasedbg/SFSCore.dll"
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw 'Build SFSCore first' }
if ((Get-Item -LiteralPath $dll).VersionInfo.FileVersion -ne "$version.0") { throw 'SFS core version mismatch' }
$dllText = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($dll))
foreach ($symbol in @('SFSOStimBridge', 'GetRedressSession', 'RestoreSession', 'BeginEquipmentPass')) {
    if (-not $dllText.Contains($symbol)) { throw "SFS core is missing $symbol" }
}
$release = Join-Path $repo 'Release'
$dist = Join-Path $repo 'dist'
$archives = @(
    (Join-Path $release "Skyrim Fitting System v$version SE-AE.zip"),
    (Join-Path $repo "Sources/Skyrim Fitting System v$version Source.zip")
)
foreach ($archive in $archives) {
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Package corresponding core/source first: $archive" }
}
$stage = Join-Path $repo ('tmp/ostim-package-v' + $patchVersion + '-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage) | Out-Null
[IO.Directory]::CreateDirectory($dist) | Out-Null
function Copy-File([string]$From, [string]$To) {
    if (-not (Test-Path -LiteralPath $From -PathType Leaf)) { throw "Missing $From" }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $To)) | Out-Null
    Copy-Item -LiteralPath $From -Destination $To
}
foreach ($variant in @('OStim', 'Standalone')) {
    $root = Join-Path $stage $variant
    foreach ($name in @('OUndress', 'SFSOStimBridge')) {
        $pex = Join-Path $patchRoot "$variant/Scripts/$name.pex"
        if (-not (Test-Path -LiteralPath $pex)) { throw "Compile $variant first" }
        $text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($pex))
        if ($text.IndexOf('SFSOStimBridge', [StringComparison]::OrdinalIgnoreCase) -lt 0) { throw "Stale compiled $pex" }
        Copy-File $pex (Join-Path $root "Scripts/$name.pex")
    }
    Copy-File (Join-Path $patchRoot "$variant/Scripts/Source/OUndress.psc") (Join-Path $root 'Scripts/Source/OUndress.psc')
    Copy-File (Join-Path $patchRoot 'shared/SFSOStimBridge.psc') (Join-Path $root 'Scripts/Source/SFSOStimBridge.psc')
    Copy-File (Join-Path $patchRoot 'README.txt') (Join-Path $root 'README.txt')
    Copy-File (Join-Path $patchRoot 'VERSION') (Join-Path $root 'VERSION')
    Copy-File (Join-Path $repo 'LICENSE') (Join-Path $root 'LICENSE.txt')
    $label = if ($variant -eq 'Standalone') { 'OStim Standalone' } else { 'OStim' }
    $archive = Join-Path $release "SFS - $label Strip Link Patch v$patchVersion.zip"
    if (Test-Path -LiteralPath $archive) { throw "Refusing to overwrite an existing package: $archive" }
    [IO.Compression.ZipFile]::CreateFromDirectory($root, $archive, [IO.Compression.CompressionLevel]::Optimal, $false)
    Copy-Item -LiteralPath $archive -Destination (Join-Path $dist (Split-Path -Leaf $archive))
    $archives += $archive
}
# The canonical complete core source archive includes BOTH patch sources,
# compile-only signatures and builders. No incomplete git-ls-files snapshot,
# separate core overlay, private experiments or compiler executables are shipped.
$hashLines = foreach ($archive in $archives) {
    "$((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash) *$(Split-Path -Leaf $archive)"
}
$hashPath = Join-Path $release "SHA256SUMS-v$version.txt"
[IO.File]::WriteAllLines($hashPath, $hashLines, [Text.UTF8Encoding]::new($false))
$hashLines | ForEach-Object { Write-Output $_ }
# Retain this uniquely named stage for inspection; no existing data is deleted.
Write-Output "Official patch stage: $stage"
