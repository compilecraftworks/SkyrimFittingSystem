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

$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.6.3 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.6.3 Source.zip'
$runtime = Read-Manifest $runtimePath
$source = Read-Manifest $sourcePath
$baseline = Read-Manifest (Join-Path $repo 'Release/Skyrim Fitting System v1.6.2 SE-AE.zip')
$changed = @(@($runtime.Keys) + @($baseline.Keys) | Sort-Object -Unique | Where-Object { $runtime[$_] -ne $baseline[$_] })
$expected = @('INSTALL.txt', 'SKSE/Plugins/SFSCore.dll',
    'Interface/SkyrimFittingSystem/locales/en.json',
    'Interface/SkyrimFittingSystem/locales/kor.json',
    'Interface/SkyrimFittingSystem/locales/zh_cn.json')
if (Compare-Object $expected $changed) { throw "Unexpected runtime changes: $($changed -join ', ')" }
$dll = Join-Path $repo 'build/v1.6.3/windows/x64/releasedbg/SFSCore.dll'
if ((Get-Item $dll).VersionInfo.FileVersion -ne '1.6.3.0' -or
    $runtime['SKSE/Plugins/SFSCore.dll'] -ne (Get-FileHash $dll).Hash) { throw 'Runtime DLL mismatch' }
foreach ($locale in @('en', 'kor', 'zh_cn')) {
    $path = "Interface/SkyrimFittingSystem/locales/$locale.json"
    if ($runtime[$path] -ne (Get-FileHash (Join-Path $repo "data/$path")).Hash) { throw "Locale mismatch: $locale" }
}
foreach ($folder in @('src', 'tests', 'scripts')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo $folder) -Recurse -File) {
        $relative = $file.FullName.Substring($repo.Length + 1).Replace('\', '/')
        if ($source[$relative] -ne (Get-FileHash $file.FullName).Hash) { throw "Source mismatch: $relative" }
    }
}
foreach ($mapping in @(@('VERSION', 'VERSION'), @('xmake.lua', 'xmake.lua.txt'),
    @('xmake-requires.lock', 'xmake-requires.lock.txt'))) {
    if ($source[$mapping[1]] -ne (Get-FileHash (Join-Path $repo $mapping[0])).Hash) { throw "Build definition mismatch: $($mapping[0])" }
}
foreach ($name in @('RELEASE-NOTES-v1.6.3.md', 'RELEASE-NOTES-v1.6.3-ko.md',
    'GITHUB-RELEASE-v1.6.3.md', 'nexus-changelog-v1.6.3-en.txt', 'nexus-changelog-v1.6.3-ko.txt',
    'V1.6.3-CONDITION-STRING-INPUT-FIX.md', 'V1.6.3-RUNTIME-RACEMENU-AUDIT.md')) {
    if ($source["docs/$name"] -ne (Get-FileHash (Join-Path $repo "docs/$name")).Hash) { throw "Release document mismatch: $name" }
}
if (@($runtime.Keys | Where-Object { $_ -match '\.dll$' }).Count -ne 1 -or
    @($runtime.Keys | Where-Object { $_ -match '\.pdb$|settings\.json$|^Data/' }).Count) { throw 'Unexpected runtime payload' }
if (@($source.Keys | Where-Object { $_ -match '\.(dll|pdb|exe|pex|esl|zip|7z)$|^(backups|private|\.tmp|\.git)/' }).Count) { throw 'Non-source/private artifact in source ZIP' }
foreach ($path in @($runtimePath, $sourcePath)) {
    if ((Get-FileHash $path).Hash -ne (Get-FileHash (Join-Path $repo ('dist/' + (Split-Path $path -Leaf)))).Hash) { throw 'Distribution ZIP mismatch' }
}
Write-Output 'v1.6.3 verified: DLL/install/locales only; scripts, helper ESL and other runtime assets unchanged.'
Write-Output 'Corresponding source, tests, scripts, build definitions, release notes and dist copies match.'
