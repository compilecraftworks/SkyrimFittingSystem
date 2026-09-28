param(
    [string]$Xmake = (Join-Path $env:USERPROFILE '.codex\external-tools\xmake\3.1.0\xmake\xmake.exe')
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $repository
try {
    # Keep -P and the working directory together: xmake 3.1.0 uses the caller's
    # working directory for configuration/generated objects when -P is given.
    & $Xmake f -P $repository -y -p windows -a x64 -m releasedbg --vs=2026 --vs_toolset=14.51.36231 --vs_sdkver=10.0.26100.0 --skyrim_se=y --skyrim_ae=y --skyrim_vr=n
    if ($LASTEXITCODE -ne 0) { throw 'Kit integration configuration failed' }
    & "$repository\tests\run-fast-regressions.ps1" -Xmake $Xmake
    & $Xmake -P $repository -y -j 6 SkyrimFittingSystem CommunityGroupingAudit
    if ($LASTEXITCODE -ne 0) { throw 'Kit integration build failed' }
} finally {
    Pop-Location
}
