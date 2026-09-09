$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repo = Split-Path -Parent $PSScriptRoot

function Read-ArchiveManifest([string]$Path) {
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    $manifest = @{}
    try {
        foreach ($entry in $archive.Entries) {
            if (-not $entry.Name) { continue }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $digest = $sha.ComputeHash($stream)
                $manifest[$entry.FullName.Replace('\', '/')] = [BitConverter]::ToString($digest).Replace('-', '')
            } finally {
                $sha.Dispose()
                $stream.Dispose()
            }
        }
    } finally { $archive.Dispose() }
    return $manifest
}

$runtimePath = Join-Path $repo 'Release/Skyrim Fitting System v1.6.1 SE-AE.zip'
$sourcePath = Join-Path $repo 'Sources/Skyrim Fitting System v1.6.1 Source.zip'
$baseline = Read-ArchiveManifest (Join-Path $repo 'Release/Skyrim Fitting System v1.6.0 SE-AE.zip')
$runtime = Read-ArchiveManifest $runtimePath
$source = Read-ArchiveManifest $sourcePath
$changed = @()
foreach ($name in (@($baseline.Keys) + @($runtime.Keys) | Sort-Object -Unique)) {
    if ($baseline[$name] -ne $runtime[$name]) { $changed += $name }
}
$allowed = @('INSTALL.txt', 'SKSE/Plugins/SFSCore.dll')
if (@($changed | Where-Object { $_ -notin $allowed }).Count -ne 0 -or $changed.Count -ne 2) {
    throw "Unexpected runtime payload changes: $($changed -join ', ')"
}
$dll = Join-Path $repo 'build/v1.6.1/windows/x64/releasedbg/SFSCore.dll'
$info = (Get-Item -LiteralPath $dll).VersionInfo
if ($info.FileVersion -ne '1.6.1.0' -or $info.ProductVersion -ne '1.6.1.0') {
    throw 'Expected a 1.6.1.0 build'
}
if ($runtime['SKSE/Plugins/SFSCore.dll'] -ne (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash) {
    throw 'Runtime archive DLL differs from the verified build output'
}
foreach ($folder in @('src', 'tests', 'scripts')) {
    foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $repo $folder) -Recurse -File)) {
        $relative = $file.FullName.Substring($repo.Length + 1).Replace('\', '/')
        if ($source[$relative] -ne (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash) {
            throw "Corresponding source is stale or missing: $relative"
        }
    }
}
foreach ($mapping in @(
    @('VERSION', 'VERSION'), @('xmake.lua', 'xmake.lua.txt'),
    @('xmake-requires.lock', 'xmake-requires.lock.txt'),
    @('docs/V1.6.1-DISPLAY-REGRESSION-AUDIT.md', 'docs/V1.6.1-DISPLAY-REGRESSION-AUDIT.md')
)) {
    if ($source[$mapping[1]] -ne (Get-FileHash -LiteralPath (Join-Path $repo $mapping[0]) -Algorithm SHA256).Hash) {
        throw "Stale corresponding source: $($mapping[0])"
    }
}
if (@($runtime.Keys | Where-Object { $_ -match '\.dll$' }).Count -ne 1 -or
    @($runtime.Keys | Where-Object { $_ -match '\.pdb$|settings\.json$' }).Count -ne 0) {
    throw 'Duplicate DLL, PDB, or user settings entered the runtime'
}
if (@($source.Keys | Where-Object { $_ -match '\.(dll|pdb|exe|pex|esl|zip|7z)$' }).Count -ne 0) {
    throw 'A compiled artifact entered the source archive'
}
foreach ($pair in @(@($runtimePath, 'dist/Skyrim Fitting System v1.6.1 SE-AE.zip'),
                    @($sourcePath, 'dist/Skyrim Fitting System v1.6.1 Source.zip'))) {
    if ((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $repo $pair[1]) -Algorithm SHA256).Hash) {
        throw "Distribution copy differs: $($pair[1])"
    }
}
Write-Output 'v1.6.1 release verified: only DLL/install guide changed; helper ESL, scripts, assets and locales are unchanged.'
Write-Output 'Runtime DLL version/hash and corresponding src/tests/scripts/build definitions match; distribution copies match.'
