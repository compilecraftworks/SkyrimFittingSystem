param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($output) | Out-Null
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
# Compile unchanged production bodies against recording providers. These are
# source regression tests, not game/RaceMenu binary emulation or timing tests.
$source = Get-Content -LiteralPath (Join-Path $repo 'src/native/RaceMenuBodyMorph.cpp') -Raw
Write-Slice 'callback.production.inc' 'class NiOverrideDispatchCallback final' 'class ScopedUpdateModelWeightTask final'
Write-Slice 'sync.production.inc' 'enum class HighHeelSyncAttempt' 'void QueuePendingMorphSync('
Write-Slice 'observer.production.inc' '[[nodiscard]] bool ShouldResyncHighHeelAfterAttachment(' 'RegisteredAppearanceAttachmentObserver g_attachmentObserver;'
Write-Slice 'queue.production.inc' 'void QueueRegisteredAppearanceHighHeelSync(RE::Actor *a_actor) {' 'void SetRegisteredAppearanceDisplayActive('
$source = Get-Content -LiteralPath (Join-Path $repo 'src/features/virtual_tokens/VirtualWornTokens.cpp') -Raw
Write-Slice 'caller-identity.production.inc' 'struct CallerIdentity {' 'struct TrustProfile {'
Write-Slice 'caller-chain.production.inc' '[[nodiscard]] std::vector<CallerIdentity>' 'void PruneRuntimeStateLocked('
Write-Slice 'target-native.production.inc' 'enum class TargetNative {' '[[nodiscard]] bool EqualNoCase('
Write-Slice 'hook-operation.production.inc' 'struct HookOperation {' '[[nodiscard]] const char *TargetName('
Write-Slice 'prepare-operation.production.inc' '[[nodiscard]] HookOperation PrepareOperation(' '[[nodiscard]] bool HandleVirtualTokenOperation('
