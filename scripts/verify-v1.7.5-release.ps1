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
    'Release/Skyrim Fitting System v1.7.4 SE-AE.zip' = '1595961F7B99FEB5826553DF60F0B3D616A466D409EAD0043A60D8FF75873B23'
    'Sources/Skyrim Fitting System v1.7.4 Source.zip' = '5ABDEB0E29F43CAB58EFDB698E327A2AE05A3145B2D661F44C6D59F817851B1B'
}
foreach ($name in $preservedArchives.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $name)).Hash -ne $preservedArchives[$name]) {
        throw "Previous release changed: $name"
    }
}
if ((Get-Content -LiteralPath (Join-Path $repo 'VERSION') -Raw).Trim() -ne '1.7.5') {
    throw 'Expected VERSION 1.7.5'
}
$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.7.5 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.7.5 Source.zip'
$runtime = Read-Manifest $runtimePath
$source = Read-Manifest $sourcePath
$baseline = Read-Manifest (Join-Path $repo 'Release/Skyrim Fitting System v1.7.4 SE-AE.zip')
$changed = @(@($runtime.Keys) + @($baseline.Keys) | Sort-Object -Unique | Where-Object { $runtime[$_] -ne $baseline[$_] })
if ($changed.Count -ne 1 -or $changed[0] -ne 'SKSE/Plugins/SFSCore.dll') {
    throw "Unexpected runtime changes: $($changed -join ', ')"
}
$dll = Join-Path $repo 'build/v1.7.5/windows/x64/releasedbg/SFSCore.dll'
if ((Get-Item -LiteralPath $dll).VersionInfo.FileVersion -ne '1.7.5.0' -or
    (Get-Item -LiteralPath $dll).VersionInfo.ProductVersion -ne '1.7.5.0' -or
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
foreach ($name in @('RELEASE-NOTES-v1.7.5.md', 'RELEASE-NOTES-v1.7.5-ko.md',
    'GITHUB-RELEASE-v1.7.5.md', 'nexus-changelog-v1.7.5-en.txt', 'nexus-changelog-v1.7.5-ko.txt',
    'SEXLAB-HIGH-HEEL-FIX-2026-10-08.md', 'nexus-description-en.md', 'nexus-description-ko.md',
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
$hashLines = Get-Content -LiteralPath (Join-Path $repo 'Release/SHA256SUMS-v1.7.5.txt')
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
$en = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.5-en.txt') -Raw).Trim()
$ko = (Get-Content -LiteralPath (Join-Path $repo 'docs/nexus-changelog-v1.7.5-ko.txt') -Raw).Trim()
$github = Get-Content -LiteralPath (Join-Path $repo 'docs/GITHUB-RELEASE-v1.7.5.md') -Raw
if (-not $github.Contains($en) -or -not $github.Contains($ko)) { throw 'Bilingual changelog mismatch' }
Write-Output 'v1.7.5 verified: only SFSCore.dll changed in runtime; ESL, scripts, locales and all other runtime assets unchanged.'
Write-Output "Corresponding source ($($source.Count) entries), pinned CommonLib, tests, docs, checksums and dist copies match."
Write-Output 'Previous v1.7.4 runtime/source ZIPs preserved.'
