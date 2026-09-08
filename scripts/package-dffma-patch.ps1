param(
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repository = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory '..'))
$patchRoot = Join-Path $repository `
    'compat\DynamicFeminineFemaleModestyAnimationsSfsPatch'
$fomodRoot = Join-Path $patchRoot 'fomod'
$moduleConfigPath = Join-Path $fomodRoot 'ModuleConfig.xml'
$infoPath = Join-Path $fomodRoot 'info.xml'
$flatModuleConfigPath = Join-Path $patchRoot 'fomod-ModuleConfig.xml'
$flatInfoPath = Join-Path $patchRoot 'fomod-info.xml'
$readmePath = Join-Path $patchRoot 'README.txt'
$packageVersion = '1.4.4.1'

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repository (
        'Release\SFS - Dynamic Feminine Female Modesty Animations OAR ' +
        "Displayed Outfit Patch v$packageVersion.zip")
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

foreach ($requiredFile in @(
    $moduleConfigPath, $infoPath, $flatModuleConfigPath, $flatInfoPath,
    $readmePath
)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required DFFMA patch file is missing: $requiredFile"
    }
}

if ((Get-FileHash -LiteralPath $moduleConfigPath -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $flatModuleConfigPath -Algorithm SHA256).Hash) {
    throw 'The canonical and source-package ModuleConfig.xml copies differ'
}
if ((Get-FileHash -LiteralPath $infoPath -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $flatInfoPath -Algorithm SHA256).Hash) {
    throw 'The canonical and source-package info.xml copies differ'
}

try {
    [xml]$moduleConfig = Get-Content -LiteralPath $moduleConfigPath -Raw
    [xml]$info = Get-Content -LiteralPath $infoPath -Raw
} catch {
    throw "DFFMA FOMOD XML is not well formed: $($_.Exception.Message)"
}

if ([string]$info.fomod.Version -ne $packageVersion) {
    throw "DFFMA info.xml version must be $packageVersion"
}

$plugins = @($moduleConfig.SelectNodes('/config/installSteps/installStep/optionalFileGroups/group/plugins/plugin'))
if ($plugins.Count -eq 0) {
    throw 'DFFMA ModuleConfig.xml contains no install choices'
}
foreach ($plugin in $plugins) {
    $elements = @($plugin.ChildNodes | Where-Object {
        $_.NodeType -eq [System.Xml.XmlNodeType]::Element
    })
    if ($elements.Count -lt 2 -or $elements[0].Name -ne 'description' -or
        [string]::IsNullOrWhiteSpace($elements[0].InnerText) -or
        $elements[$elements.Count - 1].Name -ne 'typeDescriptor') {
        throw "FOMOD choice '$($plugin.name)' violates the required description/.../typeDescriptor order"
    }
}

foreach ($folder in @($moduleConfig.SelectNodes('//folder'))) {
    $relativeSource = ([string]$folder.source).Replace([char]'\',
        [System.IO.Path]::DirectorySeparatorChar)
    $sourcePath = Join-Path $patchRoot $relativeSource
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Container)) {
        throw "FOMOD choice references a missing source folder: $($folder.source)"
    }
    if (@(Get-ChildItem -LiteralPath $sourcePath -File -Recurse).Count -eq 0) {
        throw "FOMOD choice references an empty source folder: $($folder.source)"
    }
}

$stageRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $repository "tmp\dffma-package-stage-v$packageVersion"))
$repositoryPrefix = $repository.TrimEnd('\') + '\'
if (-not $stageRoot.StartsWith(
        $repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a staging path outside the repository: $stageRoot"
}
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
[System.IO.Directory]::CreateDirectory($stageRoot) | Out-Null
Copy-Item -LiteralPath $fomodRoot -Destination (Join-Path $stageRoot 'fomod') `
    -Recurse -Force
Copy-Item -LiteralPath $readmePath -Destination (
    Join-Path $stageRoot 'README.txt') -Force

$outputDirectory = Split-Path -Parent $OutputPath
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $stageRoot, $OutputPath,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)

$archive = [System.IO.Compression.ZipFile]::OpenRead($OutputPath)
try {
    $entryNames = @($archive.Entries | ForEach-Object {
        $_.FullName.Replace([char]'\', [char]'/')
    })
    if (-not ($entryNames -contains 'fomod/ModuleConfig.xml') -or
        -not ($entryNames -contains 'fomod/info.xml') -or
        -not ($entryNames -contains 'README.txt')) {
        throw 'Packaged DFFMA archive has an invalid FOMOD root layout'
    }
} finally {
    $archive.Dispose()
}

$hash = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash
$hashPath = [System.IO.Path]::ChangeExtension($OutputPath, '.sha256.txt')
[System.IO.File]::WriteAllText(
    $hashPath,
    "$hash *$([System.IO.Path]::GetFileName($OutputPath))`r`n",
    [System.Text.UTF8Encoding]::new($false))

Write-Host "DFFMA patch: $OutputPath"
Write-Host "SHA-256:     $hash"
