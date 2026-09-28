param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$target = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($target) | Out-Null
function Emit([string]$Name, [string]$Text) {
    $path = Join-Path $target $Name
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $Text) {
        [IO.File]::WriteAllText($path, $Text, [Text.UTF8Encoding]::new($false))
    }
}
$hooks = [IO.File]::ReadAllText((Join-Path $repo 'src/Hooks.cpp')).Replace("`r`n", "`n")
$start = $hooks.IndexOf('std::mutex g_shortcutFilterMutex;')
$end = $hooks.IndexOf("static void`nhk_PollInputDevices", $start)
if ($start -lt 0 -or $end -lt $start) { throw 'Missing production filter boundaries' }
Emit 'InputFilter.production.inc' $hooks.Substring($start, $end - $start)
$menu = [IO.File]::ReadAllText((Join-Path $repo 'src/ui/Menu.Visibility.cpp')).Replace("`r`n", "`n")
$start = $menu.IndexOf('bool Menu::IsKeyboardListNavigationActive() const')
$end = $menu.IndexOf('bool Menu::QueueKitListCommand(', $start)
if ($start -lt 0 -or $end -lt $start) { throw 'Missing production navigation boundaries' }
Emit 'NavigationContext.production.inc' $menu.Substring($start, $end - $start)
$start = $hooks.IndexOf('void hk_PollInputDevices(')
$end = $hooks.IndexOf('struct WndProcHook', $start)
if ($start -lt 0 -or $end -lt $start) { throw 'Missing production dispatch boundaries' }
Emit 'InputDispatch.production.inc' $hooks.Substring($start, $end - $start)
