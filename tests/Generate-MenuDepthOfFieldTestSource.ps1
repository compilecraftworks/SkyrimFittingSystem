param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = Get-Content -LiteralPath (Join-Path $repo 'src/ui/MenuDepthOfField.cpp') -Raw
$start = $source.IndexOf('std::atomic_bool g_menuOpen', [StringComparison]::Ordinal)
$end = $source.IndexOf('void ConnectEnb()', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Missing production DOF adapter boundaries' }
[IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($OutputDirectory)) | Out-Null
$path = Join-Path $OutputDirectory 'MenuDepthOfField.production.inc'
$content = $source.Substring($start, $end - $start)
if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $content) {
    [IO.File]::WriteAllText($path, $content, [Text.UTF8Encoding]::new($false))
}
