param([Parameter(Mandatory=$true)][string]$UpstreamRepository)
$ErrorActionPreference = 'Stop'
# Read-only source evidence. This does not download/build/load RaceMenu and is
# not a substitute for matching distributed DLLs to upstream source commits.
$pin = '9ebcb733e17be695f994cd2e9cc383043446bc02'
function Read-GitFile([string]$Commit, [string]$Path) {
    $lines = & git -C $UpstreamRepository show "${Commit}:$Path" 2>$null
    if ($LASTEXITCODE -ne 0) { return '' }
    return ($lines -join "`n")
}
function Class-Body([string]$Source, [string]$Name) {
    $sourceText = [regex]::Replace($Source, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')
    $start = [regex]::Match($sourceText, '\bclass\s+' + $Name + '\s*:[^{]+\{')
    if (-not $start.Success) { return '' }
    $offset = $start.Index + $start.Length
    $depth = 1
    for ($i = $offset; $i -lt $sourceText.Length; ++$i) {
        if ($sourceText[$i] -eq '{') { ++$depth }
        if ($sourceText[$i] -eq '}') { --$depth }
        if ($depth -eq 0) { return $sourceText.Substring($offset, $i - $offset) }
    }
    throw "Unterminated class $Name"
}
function Top-LevelVirtuals([string]$Body) {
    # Remove nested visitor classes/inline bodies before counting declarations.
    $flat = [System.Text.StringBuilder]::new()
    $depth = 0
    foreach ($character in $Body.ToCharArray()) {
        if ($character -eq '{') { ++$depth }
        elseif ($character -eq '}') { --$depth }
        elseif ($depth -eq 0) { [void]$flat.Append($character) }
    }
    return @([regex]::Matches($flat.ToString(), '\bvirtual\s+[^;{}]*?\b(\w+)\s*\([^;{}]*\)[^;{}]*;') |
        ForEach-Object { $_.Groups[1].Value })
}
$commits = @(& git -C $UpstreamRepository log --reverse --format=%H $pin -- `
    skee/IPluginInterface.h skee/BodyMorphInterface.h skee/ActorUpdateManager.h skee/NiTransformInterface.h `
    skee64/IPluginInterface.h skee64/BodyMorphInterface.h skee64/ActorUpdateManager.h skee64/NiTransformInterface.h)
if ($LASTEXITCODE -ne 0 -or $commits.Count -eq 0) { throw 'Missing pinned upstream history' }
$publicCount = 0
foreach ($commit in $commits) {
    $directory = 'skee'
    $paths = @(& git -C $UpstreamRepository ls-tree --name-only $commit)
    if ($paths -contains 'skee64') { $directory = 'skee64' }
    $public = Read-GitFile $commit "$directory/IPluginInterface.h"
    $concrete = Read-GitFile $commit "$directory/BodyMorphInterface.h"
    $version = [regex]::Match((Class-Body $concrete 'BodyMorphInterface'), 'kCurrentPluginVersion\s*=\s*(\d+)').Groups[1].Value
    $body = Class-Body $public 'IBodyMorphInterface'
    if (-not $version -and $body) {
        $version = [regex]::Match($body, 'kCurrentPluginVersion\s*=\s*kPluginVersion(\d+)').Groups[1].Value
    }
    if (-not $version) { throw "Missing BodyMorph version at $commit" }
    if ($body) {
        $slots = @(Top-LevelVirtuals $body)
        foreach ($expected in @(@('GetBodyMorphs', 6), @('VisitMorphs', 8), @('ApplyVertexDiff', 12), @('ApplyBodyMorphs', 13))) {
            $actual = [array]::IndexOf($slots, $expected[0]) + 3
            if ($actual -ne $expected[1]) { throw "$commit $($expected[0]): slot $actual != $($expected[1])" }
        }
        ++$publicCount
        Write-Output "$commit BodyMorph=$version public: used slots 6/8/12/13 PASS"
    } else {
        Write-Output "$commit BodyMorph=$version historical concrete ABI (not public v4)"
    }
}
Write-Output "Audited $($commits.Count) interface/task change snapshots; $publicCount public-prefix snapshots passed. Pin=$pin. Package-to-binary correspondence remains separate evidence."
