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
    'Release/Skyrim Fitting System v1.7.5 SE-AE.zip' = '25834D00975720E80DFC637D624FCB1FCC19CF4100F039B746C9F7FCEDB81DD0'
    'Sources/Skyrim Fitting System v1.7.5 Source.zip' = '1DE3559C0CA2FC336B459F0F60F22D44733A8A89C846822FAA7DEF68FCCB3163'
}
foreach ($name in $preservedArchives.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $preservedArchives[$name]) {
        throw "Previous release changed: $name"
    }
}
if ((Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim() -ne '1.7.6') {
    throw 'Expected VERSION 1.7.6'
}
$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.7.6 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.7.6 Source.zip'
$runtime = Read-Manifest $runtimePath
$source = Read-Manifest $sourcePath
$baseline = Read-Manifest (Join-Path $repo 'Release/Skyrim Fitting System v1.7.5 SE-AE.zip')
$changed = @(@($runtime.Keys) + @($baseline.Keys) | Sort-Object -Unique | Where-Object { $runtime[$_] -ne $baseline[$_] })
if (($changed -join '|') -ne 'SKSE/Plugins/SFSCore.dll|THIRD_PARTY_NOTICES.txt') {
    throw "Unexpected runtime changes: $($changed -join ', ')"
}
$dll = Join-Path $repo 'build/v1.7.6/windows/x64/releasedbg/SFSCore.dll'
if ((Get-Item -LiteralPath $dll).VersionInfo.FileVersion -ne '1.7.6.0' -or
    (Get-Item -LiteralPath $dll).VersionInfo.ProductVersion -ne '1.7.6.0' -or
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
foreach ($name in @('RELEASE-NOTES-v1.7.6.md', 'RELEASE-NOTES-v1.7.6-ko.md',
    'GITHUB-RELEASE-v1.7.6.md', 'nexus-changelog-v1.7.6-en.txt', 'nexus-changelog-v1.7.6-ko.txt',
    'OStim-Strip-Link-Implementation-2026-10-09.md', 'nexus-description-en.md', 'nexus-description-ko.md',
    'nexus-description.bbcode', 'nexus-description-ko.bbcode', 'tullius-description-ko.html',
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
$hashLines = Get-Content -LiteralPath (Join-Path $repo 'Release/SHA256SUMS-v1.7.6.txt')
foreach ($path in @($runtimePath, $sourcePath)) {
    $hash = (Get-FileHash -LiteralPath $path).Hash
    $name = Split-Path $path -Leaf
    if ($hash -ne (Get-FileHash -LiteralPath (Join-Path $repo ('dist/' + $name))).Hash) { throw 'Distribution ZIP mismatch' }
    if ($hashLines -notcontains "$hash *$name") { throw "SHA256SUMS mismatch: $name" }
}
foreach ($locale in @('en', 'kor', 'zh_cn')) {
    $relative = "Interface/SkyrimFittingSystem/locales/$locale.json"
    $current = (Get-FileHash -LiteralPath (Join-Path $repo "data/$relative")).Hash
    if ($runtime[$relative] -ne $current -or $source["data/$relative"] -ne $current) { throw "Locale mismatch: $locale" }
}
$en = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.6-en.txt') -Raw).Trim()
$ko = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.6-ko.txt') -Raw).Trim()
$github = Get-Content -LiteralPath (Join-Path $repo 'docs/GITHUB-RELEASE-v1.7.6.md') -Raw
if (-not $github.Contains($en) -or -not $github.Contains($ko)) { throw 'Bilingual changelog mismatch' }
Write-Output 'v1.7.6 verified: only SFSCore.dll and third-party notice changed; ESL, scripts, locales and all other runtime assets unchanged.'
Write-Output "Corresponding source ($($source.Count) entries), pinned CommonLib, tests, docs, checksums and dist copies match."
Write-Output 'Previous v1.7.5 runtime/source ZIPs preserved.'

foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'compat/OStimSfsPatch') -Recurse -File) {
    if ($file.Extension -ne '.psc' -and $file.Name -notin @('README.txt','VERSION')) { continue }
    $relative = $file.FullName.Substring($repo.Length+1).Replace('\','/')
    if ($source[$relative] -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
        throw "Missing/mismatched corresponding patch source: $relative"
    }
}
$patchVersion = (Get-Content -LiteralPath (Join-Path $repo 'compat/OStimSfsPatch/VERSION') -Raw).Trim()
if ($patchVersion -ne '1.0.0') { throw 'Expected official patches 1.0.0' }
$readme = Get-Content -LiteralPath (Join-Path $repo 'compat/OStimSfsPatch/README.txt') -Raw
if (-not $readme.Contains('Official release: 1.0.0') -or
    -not $readme.Contains('SFS v1.7.6 or later') -or $readme -match '1.0.0-test|Local test build') {
    throw 'Stale patch release metadata'
}
foreach ($variant in @('OStim','Standalone')) {
    $label = if ($variant -eq 'Standalone') { 'OStim Standalone' } else { 'OStim' }
    $path = Join-Path $repo "Release/SFS - $label Strip Link Patch v1.0.0.zip"
    $patch = Read-Manifest $path
    $expected = @('Scripts/OUndress.pex','Scripts/SFSOStimBridge.pex',
        'Scripts/Source/OUndress.psc','Scripts/Source/SFSOStimBridge.psc','README.txt','VERSION','LICENSE.txt')
    if ((($patch.Keys | Sort-Object) -join '|') -cne (($expected | Sort-Object) -join '|')) {
        throw "Unexpected patch payload: $label"
    }
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
    $hash = (Get-FileHash -LiteralPath $path).Hash
    $name = Split-Path $path -Leaf
    if ($hash -ne (Get-FileHash -LiteralPath (Join-Path $repo ('dist/'+$name))).Hash -or
        $hashLines -notcontains "$hash *$name") { throw "Patch checksum/dist mismatch: $label" }
}
if ($hashLines.Count -ne 4) { throw 'Manifest must describe core, source and exactly two official patches' }
Write-Output 'Both official OStim v1.0.0 patches verified against compiled PEX and corresponding PSC/license/version; no DLL, ESP, compile-only runtime files or extra payload.'
