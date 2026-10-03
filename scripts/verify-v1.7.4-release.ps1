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
    'Release/Skyrim Fitting System v1.7.3 SE-AE.zip' = '2B6D935D088F9D43012FE920C24A4AC8388A33F78557E8FB7AAEF2C5CAF14E5B'
    'Sources/Skyrim Fitting System v1.7.3 Source.zip' = 'F074C0ECDDA0EED80C9E30DE65A90E263B36B3FD17894F1C8CD9EC7C0673EC9B'
}
foreach ($name in $preservedArchives.Keys) {
    if ((Get-FileHash (Join-Path $repo $name)).Hash -ne $preservedArchives[$name]) {
        throw "Previous release changed: $name"
    }
}
$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.7.4 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.7.4 Source.zip'
$runtime = Read-Manifest $runtimePath
$source = Read-Manifest $sourcePath
$baseline = Read-Manifest (Join-Path $repo 'Release/Skyrim Fitting System v1.7.3 SE-AE.zip')
$changed = @(@($runtime.Keys) + @($baseline.Keys) | Sort-Object -Unique | Where-Object { $runtime[$_] -ne $baseline[$_] })
if ($changed.Count -ne 1 -or $changed[0] -ne 'SKSE/Plugins/SFSCore.dll') {
    throw "Unexpected runtime changes: $($changed -join ', ')"
}
$dll = Join-Path $repo 'build/v1.7.4/windows/x64/releasedbg/SFSCore.dll'
if ((Get-Item $dll).VersionInfo.FileVersion -ne '1.7.4.0' -or
    (Get-Item $dll).VersionInfo.ProductVersion -ne '1.7.4.0' -or
    $runtime['SKSE/Plugins/SFSCore.dll'] -ne (Get-FileHash $dll).Hash) { throw 'Runtime DLL mismatch' }
foreach ($folder in @('src', 'tests', 'scripts', 'extras', 'third_party/CommonLibSSE-NG')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo $folder) -Recurse -File) {
        $relative = $file.FullName.Substring($repo.Length + 1).Replace('\', '/')
        if ($source[$relative] -ne (Get-FileHash $file.FullName).Hash) { throw "Source mismatch: $relative" }
    }
}
foreach ($mapping in @(@('VERSION', 'VERSION'), @('xmake.lua', 'xmake.lua.txt'),
    @('xmake-requires.lock', 'xmake-requires.lock.txt'), @('README.md', 'README.md'),
    @('RELEASE_SOURCE_NOTICE.txt', 'RELEASE_SOURCE_NOTICE.txt'),
    @('SOURCE_PACKAGE_README.txt', 'SOURCE_PACKAGE_README.txt'))) {
    if ($source[$mapping[1]] -ne (Get-FileHash (Join-Path $repo $mapping[0])).Hash) { throw "Build definition mismatch: $($mapping[0])" }
}
foreach ($name in @('RELEASE-NOTES-v1.7.4.md', 'RELEASE-NOTES-v1.7.4-ko.md',
    'GITHUB-RELEASE-v1.7.4.md', 'nexus-changelog-v1.7.4-en.txt', 'nexus-changelog-v1.7.4-ko.txt',
    'ACTUAL-EQUIPMENT-CONFLICT-FIX-2026-10-03.md',
    'nexus-description-en.md', 'nexus-description-ko.md',
    'nexus-description.bbcode', 'nexus-description-ko.bbcode')) {
    if ($source["docs/$name"] -ne (Get-FileHash (Join-Path $repo "docs/$name")).Hash) { throw "Release document mismatch: $name" }
}
if (@($runtime.Keys | Where-Object { $_ -match '\.dll$' }).Count -ne 1 -or
    @($runtime.Keys | Where-Object { $_ -match '\.pdb$|settings\.json$|^Data/' }).Count) { throw 'Unexpected runtime payload' }
if (@($source.Keys | Where-Object { $_ -match '\.(dll|pdb|exe|pex|esl|zip|7z|xlsx|pyc)$|^(backups|private|kit-patch|\.tmp|\.git|output)/|(^|/)__pycache__/' }).Count) { throw 'Non-source/private artifact in source ZIP' }
$hashLines = Get-Content -LiteralPath (Join-Path $repo 'Release/SHA256SUMS-v1.7.4.txt')
foreach ($path in @($runtimePath, $sourcePath)) {
    $hash = (Get-FileHash $path).Hash
    $name = Split-Path $path -Leaf
    if ($hash -ne (Get-FileHash (Join-Path $repo ('dist/' + $name))).Hash) { throw 'Distribution ZIP mismatch' }
    if ($hashLines -notcontains "$hash *$name") { throw "SHA256SUMS mismatch: $name" }
}
foreach ($locale in @('en', 'kor', 'zh_cn')) {
    $relative = "Interface/SkyrimFittingSystem/locales/$locale.json"
    $current = (Get-FileHash (Join-Path $repo "data/$relative")).Hash
    if ($runtime[$relative] -ne $current -or $source["data/$relative"] -ne $current) {
        throw "Locale mismatch: $locale"
    }
}
Write-Output 'v1.7.4 verified: only SFSCore.dll changed in runtime; ESL, scripts, locales and all other runtime assets unchanged.'
Write-Output "Corresponding source ($($source.Count) entries), pinned CommonLib, tests, docs, checksums and dist copies match."
Write-Output 'Previous v1.7.3 runtime/source ZIPs preserved.'
