param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$target = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($target) | Out-Null
function Read-Source([string]$Path) {
    [IO.File]::ReadAllText((Join-Path $repo $Path)).Replace("`r`n", "`n")
}
function Extract([string]$Path, [string]$Signature) {
    $source = Read-Source $Path
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing $Signature" }
    $open = $source.IndexOf('{', $start)
    $depth = 0
    for ($i=$open; $i -lt $source.Length; ++$i) {
        if ($source[$i] -eq '{') { ++$depth }
        if ($source[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $source.Substring($start, $i+1-$start) }
        }
    }
    throw "Unbalanced $Signature"
}
function Emit([string]$Name, [string]$Text) {
    $path = Join-Path $target $Name
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $Text) {
        [IO.File]::WriteAllText($path, $Text, [Text.UTF8Encoding]::new($false))
    }
}
$resolver = Read-Source 'src/native/GenitalArmorResolver.cpp'
Emit 'Resolver.production.inc' ([regex]::Replace($resolver, '(?m)^#include "[^"\n]+"\n', ''))
Emit 'ControlLease.production.inc' (@(
    (Extract 'src/ui/Menu.Visibility.cpp' 'void AcquireGameplayControls()'),
    (Extract 'src/ui/Menu.Visibility.cpp' 'void ReleaseGameplayControls()')
) -join "`n`n")
Emit 'Menu.production.inc' (@(
    (Extract 'src/ui/Menu.Visibility.cpp' 'void Menu::OnMenuShow()'),
    (Extract 'src/ui/Menu.Visibility.cpp' 'void Menu::OnMenuHide()'),
    (Extract 'src/ui/Menu.Visibility.cpp' 'void Menu::NotifyWindowShutdown()'),
    (Extract 'src/ui/Menu.Browser.cpp' 'void Menu::ClearCatalogSelection()')
) -join "`n`n")
Emit 'Kit.production.inc' (Extract 'src/ui/Menu.Kits.cpp' 'std::string NormalizeKitCollection(')
$conditions = Read-Source 'src/ui/Menu.Conditions.Serialization.cpp'
$conditions = $conditions.Substring($conditions.IndexOf('namespace {'))
Emit 'Conditions.production.inc' $conditions.Substring(0, $conditions.IndexOf('} // namespace') + '} // namespace'.Length)
