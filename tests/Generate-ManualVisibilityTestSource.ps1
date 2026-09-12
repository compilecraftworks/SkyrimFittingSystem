param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$target = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($target) | Out-Null
$parts = @(
    [pscustomobject]@{Source='src/features/virtual_tokens/VirtualWornTokens.cpp'; Name='ManualVisibilityTickets.production.inc'; Start='void InvalidateAppearanceTickets('; End='enum class TargetNative'},
    [pscustomobject]@{Source='src/features/virtual_tokens/VirtualWornTokens.cpp'; Name='ManualVisibilityTickets.production.inc'; Start='void CompleteSettledTransactions('; End='void QueueSettledTransactionCheck('},
    [pscustomobject]@{Source='src/VariantWorkbench.cpp'; Name='ManualVisibilitySetter.production.inc'; Start='bool VariantWorkbench::SetOverrideHidden('; End='bool VariantWorkbench::SetOverrideLocked('}
)
foreach ($group in ($parts | Group-Object Name)) {
    $blocks = foreach ($part in $group.Group) {
        $source = (Get-Content -LiteralPath (Join-Path $repo $part.Source) -Raw).Replace("`r`n", "`n")
        $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
        if ($start -lt 0) { throw "Missing definition: $($part.Start)" }
        $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
        if ($end -le $start) { throw "Missing boundary: $($part.End)" }
        $source.Substring($start, $end - $start)
    }
    $content = $blocks -join "`n"
    $path = Join-Path $target $group.Name
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
        [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
    }
}
