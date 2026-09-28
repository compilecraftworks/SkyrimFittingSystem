param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$groups = @{
    'MaterializerDeclaration.production.inc' = @('src/ConditionMaterializer.h', 'namespace sfs::conditions {')
    'MaterializerState.production.inc' = @('src/conditions/MaterializationState.h', 'namespace sfs::conditions {')
    'Materializer.production.inc' = @('src/ConditionMaterializer.cpp', 'namespace {')
}
foreach ($entry in $groups.GetEnumerator()) {
    $source = Get-Content -LiteralPath (Join-Path $repo $entry.Value[0]) -Raw
    $start = $source.IndexOf($entry.Value[1], [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production namespace: $($entry.Value[0])" }
    # Compile the complete production declarations/cache/invalidation/accessors;
    # only engine emission, definition lookup and status boundaries are fixtures.
    $content = $source.Substring($start)
    $path = [IO.Path]::GetFullPath((Join-Path $OutputDirectory $entry.Key))
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
        [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
    }
}
