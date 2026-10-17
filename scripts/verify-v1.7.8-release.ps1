$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = Split-Path -Parent $PSScriptRoot

function Read-Manifest([string]$Path) {
    $zip = [IO.Compression.ZipFile]::OpenRead($Path)
    $manifest = @{}
    try {
        foreach ($entry in $zip.Entries) {
            if (-not $entry.Name) { continue }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $name = $entry.FullName.Replace('\', '/')
                if ($manifest.ContainsKey($name)) { throw "Duplicate ZIP entry: $name" }
                $manifest[$name] = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
            } finally { $sha.Dispose(); $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    return $manifest
}

$preservedArchives = @{
    'Release/Skyrim Fitting System v1.7.7 SE-AE.zip' = '439DFAC5F086764BFE723BD0A8D5043F25216301456A9C352A75AD5F25D092E7'
    'Sources/Skyrim Fitting System v1.7.7 Source.zip' = '81CC6FB7F49B037A923A51D6DD0AACD5606954FDE2BC8C9A7A5BCDDCC7E1A99A'
    'Release/SFS - OStim Strip Link Patch v1.0.0.zip' = 'CDAF010D296A0B204C68FC9BF3518444DDAF2210AE331F3697DF5967396E94FA'
    'Release/SFS - OStim Standalone Strip Link Patch v1.0.0.zip' = 'BF3ECF8C41B3973A1355FDE494A4D4A24ABE00E260850C75ACDE1EFE8FFACB38'
}
foreach ($name in $preservedArchives.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $preservedArchives[$name]) {
        throw "Published archive changed: $name"
    }
}
if ((Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim() -ne '1.7.8') {
    throw 'Expected VERSION 1.7.8'
}
$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.7.8 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.7.8 Source.zip'
$runtime = Read-Manifest $runtimePath
$source = Read-Manifest $sourcePath
$baseline = Read-Manifest (Join-Path $repo 'Release/Skyrim Fitting System v1.7.7 SE-AE.zip')
$changed = @(@($runtime.Keys) + @($baseline.Keys) | Sort-Object -Unique | Where-Object { $runtime[$_] -ne $baseline[$_] })
if (($changed -join '|') -ne 'SKSE/Plugins/SFSCore.dll') {
    throw "Unexpected runtime changes: $($changed -join ', ')"
}
$dll = Join-Path $repo 'build/v1.7.8/windows/x64/releasedbg/SFSCore.dll'
$info = (Get-Item -LiteralPath $dll).VersionInfo
if ($info.FileVersion -ne '1.7.8.0' -or $info.ProductVersion -ne '1.7.8.0' -or
    $runtime['SKSE/Plugins/SFSCore.dll'] -ne (Get-FileHash -LiteralPath $dll).Hash) { throw 'Runtime DLL mismatch' }
foreach ($folder in @('src', 'tests', 'scripts', 'extras', 'third_party/CommonLibSSE-NG')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo $folder) -Recurse -File) {
        $relative = $file.FullName.Substring($repo.Length + 1).Replace('\', '/')
        if ($source[$relative] -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "Source mismatch: $relative" }
    }
}
foreach ($mapping in @(@('VERSION', 'VERSION'), @('xmake.lua', 'xmake.lua.txt'),
    @('xmake-requires.lock', 'xmake-requires.lock.txt'), @('README.md', 'README.md'),
    @('RELEASE_SOURCE_NOTICE.txt', 'RELEASE_SOURCE_NOTICE.txt'),
    @('SOURCE_PACKAGE_README.txt', 'SOURCE_PACKAGE_README.txt'),
    @('DEPENDENCIES.md', 'DEPENDENCIES.md'), @('THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES.md'))) {
    if ($source[$mapping[1]] -ne (Get-FileHash -LiteralPath (Join-Path $repo $mapping[0])).Hash) {
        throw "Build definition mismatch: $($mapping[0])"
    }
}
foreach ($name in @('RELEASE-NOTES-v1.7.8.md', 'RELEASE-NOTES-v1.7.8-ko.md',
    'GITHUB-RELEASE-v1.7.8.md', 'nexus-changelog-v1.7.8-en.txt', 'nexus-changelog-v1.7.8-ko.txt',
    'HIGH-HEEL-RESCAN-AND-MENU-DOF.md', 'OStim-Strip-Link-Implementation-2026-10-09.md',
    'nexus-description-en.md', 'nexus-description-ko.md', 'nexus-description.bbcode',
    'nexus-description-ko.bbcode', 'tullius-description-ko.html',
    'nexus-update-history-all-en.md', 'nexus-update-history-all-ko.md')) {
    if ($source["docs/$name"] -ne (Get-FileHash -LiteralPath (Join-Path $repo "docs/$name")).Hash) {
        throw "Release document mismatch: $name"
    }
}
if (@($runtime.Keys | Where-Object { $_ -match '\.dll$' }).Count -ne 1 -or
    @($runtime.Keys | Where-Object { $_ -match '\.pdb$|settings\.json$|^Data/' }).Count) { throw 'Unexpected runtime payload' }
if (@($source.Keys | Where-Object { $_ -match '\.(dll|pdb|exe|pex|esl|zip|7z|xlsx|pyc)$|^(backups|private|kit-patch|\.tmp|\.git|output)/|(^|/)__pycache__/' }).Count) {
    throw 'Non-source/private artifact in source ZIP'
}
foreach ($locale in @('en', 'kor', 'zh_cn')) {
    $relative = "Interface/SkyrimFittingSystem/locales/$locale.json"
    $current = (Get-FileHash -LiteralPath (Join-Path $repo "data/$relative")).Hash
    if ($runtime[$relative] -ne $current -or $source["data/$relative"] -ne $current) { throw "Locale mismatch: $locale" }
}
$en = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.8-en.txt') -Raw).Trim()
$ko = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.8-ko.txt') -Raw).Trim()
$github = Get-Content -LiteralPath (Join-Path $repo 'docs/GITHUB-RELEASE-v1.7.8.md') -Raw
if (-not $github.Contains($en) -or -not $github.Contains($ko)) { throw 'Bilingual changelog mismatch' }

$hashLines = Get-Content -LiteralPath (Join-Path $repo 'Release/SHA256SUMS-v1.7.8.txt')
$published = @($runtimePath, $sourcePath)
foreach ($variant in @('OStim','Standalone')) {
    $label = if ($variant -eq 'Standalone') { 'OStim Standalone' } else { 'OStim' }
    $path = Join-Path $repo "Release/SFS - $label Strip Link Patch v1.0.0.zip"
    $patch = Read-Manifest $path
    $expected = @('Scripts/OUndress.pex','Scripts/SFSOStimBridge.pex',
        'Scripts/Source/OUndress.psc','Scripts/Source/SFSOStimBridge.psc','README.txt','VERSION','LICENSE.txt')
    if ((($patch.Keys | Sort-Object) -join '|') -cne (($expected | Sort-Object) -join '|')) { throw "Unexpected patch payload: $label" }
    foreach ($mapping in @(
        @("compat/OStimSfsPatch/$variant/Scripts/OUndress.pex",'Scripts/OUndress.pex'),
        @("compat/OStimSfsPatch/$variant/Scripts/SFSOStimBridge.pex",'Scripts/SFSOStimBridge.pex'),
        @("compat/OStimSfsPatch/$variant/Scripts/Source/OUndress.psc",'Scripts/Source/OUndress.psc'),
        @('compat/OStimSfsPatch/shared/SFSOStimBridge.psc','Scripts/Source/SFSOStimBridge.psc'),
        @('compat/OStimSfsPatch/README.txt','README.txt'),
        @('compat/OStimSfsPatch/VERSION','VERSION'), @('LICENSE','LICENSE.txt')
    )) {
        if ($patch[$mapping[1]] -ne (Get-FileHash -LiteralPath (Join-Path $repo $mapping[0])).Hash) {
            throw "Patch/workspace mismatch: $label / $($mapping[1])"
        }
    }
    $published += $path
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'compat/OStimSfsPatch') -Recurse -File) {
    if ($file.Extension -ne '.psc' -and $file.Name -notin @('README.txt','VERSION')) { continue }
    $relative = $file.FullName.Substring($repo.Length+1).Replace('\','/')
    if ($source[$relative] -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw "Patch source mismatch: $relative" }
}
foreach ($path in $published) {
    $hash = (Get-FileHash -LiteralPath $path).Hash
    $name = Split-Path -Leaf $path
    if ($hash -ne (Get-FileHash -LiteralPath (Join-Path $repo ('dist/' + $name))).Hash) { throw 'Distribution ZIP mismatch' }
    if ($hashLines -notcontains "$hash *$name") { throw "SHA256SUMS mismatch: $name" }
}
if ($hashLines.Count -ne 4) { throw 'Manifest must describe core, source and two unchanged optional patches' }
Write-Output 'v1.7.8 verified: only SFSCore.dll changed; all other runtime assets and existing optional patches are byte-identical.'
Write-Output "Corresponding source ($($source.Count) entries), pinned dependencies, tests, release documents, checksums and dist copies match."
Write-Output 'Previous v1.7.7 runtime/source archives preserved; official OStim patches remain v1.0.0.'
