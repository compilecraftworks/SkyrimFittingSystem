param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($output) | Out-Null
$source = Get-Content -LiteralPath (Join-Path $repo 'src/features/virtual_tokens/VirtualWornTokens.cpp') -Raw
function Write-Slice([string]$Name, [string]$Start, [string]$End) {
    $first = $source.IndexOf($Start, [StringComparison]::Ordinal)
    if ($first -lt 0) { throw "Missing production start: $Start" }
    $last = $source.IndexOf($End, $first + $Start.Length, [StringComparison]::Ordinal)
    if ($last -le $first) { throw "Missing production end: $End" }
    $content = $source.Substring($first, $last - $first)
    $path = Join-Path $output $Name
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
        [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
    }
}
# Actual production traversal, memo, queue, and hooks; engine/task boundaries
# are controlled in the host test. No game ABI/timing claim.
Write-Slice 'type-inspection.production.inc' 'bool PatchSelectedNativesInType(' '[[nodiscard]] std::size_t InspectFullyLinkedSexLabPPlusAliasMembers('
Write-Slice 'type-lookup.production.inc' 'void QueuePostLinkTypeInspection(' '[[nodiscard]] bool InstallScriptTypeLoadHook('
Write-Slice 'native-registration.production.inc' 'struct NativeRegistrationHook {' '[[nodiscard]] bool InstallNativeRegistrationHook('
