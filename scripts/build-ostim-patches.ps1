param(
    [string]$Compiler = (Join-Path $PSScriptRoot '../tools/papyrus-compiler/Original Compiler/PapyrusCompiler.exe'),
    [string]$GameSources = 'D:\TuLED\File Mod Skyrim SE\mods\Creation Kit - Source\Scripts\Source'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$patch = Join-Path $repo 'compat/OStimSfsPatch'
$flags = Join-Path (Split-Path -Parent $Compiler) 'TESV_Papyrus_Flags.flg'
foreach ($required in @($Compiler, $flags, (Join-Path $GameSources 'Actor.psc'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing build input: $required" }
}
$imports = "$(Join-Path $patch 'shared');$(Join-Path $patch 'compile-only');$GameSources"
foreach ($variant in @('OStim', 'Standalone')) {
    $root = Join-Path $patch $variant
    $output = Join-Path $root 'Scripts'
    $variantImports = "$(Join-Path $root 'Scripts/Source');$imports"
    [IO.Directory]::CreateDirectory($output) | Out-Null
    foreach ($source in @((Join-Path $root 'Scripts/Source/OUndress.psc'), (Join-Path $patch 'shared/SFSOStimBridge.psc'))) {
        & $Compiler $source "-i=$variantImports" "-o=$output" "-f=$flags" -optimize
        if ($LASTEXITCODE -ne 0) { throw "Papyrus compilation failed: $variant / $source" }
    }
    foreach ($name in @('OUndress', 'SFSOStimBridge')) {
        $pex = Join-Path $output "$name.pex"
        if (-not (Test-Path -LiteralPath $pex)) { throw "Missing compiled $pex" }
    }
}
Write-Host 'Both OStim patch variants compiled. Compile-only imports are NOT runtime patch files.'
