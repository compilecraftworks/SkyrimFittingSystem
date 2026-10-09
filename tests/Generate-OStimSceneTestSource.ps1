param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/native/OStimIntegration.cpp'))
$start = $source.IndexOf('namespace sfs::native::ostim {', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Missing production OStim adapter namespace' }
# Keep the complete real adapter: ABI declarations, listener, dispatch and gates.
# Only external Win32/SKSE/engine headers are replaced by the host boundaries.
$text = $source.Substring($start)
$absolute = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory((Split-Path -Parent $absolute)) | Out-Null
if (-not [IO.File]::Exists($absolute) -or [IO.File]::ReadAllText($absolute) -cne $text) {
    [IO.File]::WriteAllText($absolute, $text, [Text.UTF8Encoding]::new($false))
}
