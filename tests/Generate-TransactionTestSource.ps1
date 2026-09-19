param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$target = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($target) | Out-Null

function Read-Source([string]$Path) {
    return [IO.File]::ReadAllText((Join-Path $repo $Path)).Replace("`r`n", "`n")
}
function Extract-Function([string]$Path, [string]$Signature) {
    $source = Read-Source $Path
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing definition: $Signature" }
    $open = $source.IndexOf('{', $start)
    $depth = 0
    for ($i = $open; $i -lt $source.Length; ++$i) {
        if ($source[$i] -eq '{') { ++$depth }
        if ($source[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $source.Substring($start, $i + 1 - $start) }
        }
    }
    throw "Unbalanced definition: $Signature"
}
function Write-Generated([string]$Name, [string]$Content) {
    $path = Join-Path $target $Name
    if (-not [IO.File]::Exists($path) -or [IO.File]::ReadAllText($path) -cne $Content) {
        [IO.File]::WriteAllText($path, $Content, [Text.UTF8Encoding]::new($false))
    }
}

$workbench = @(
    (Extract-Function 'src/VariantWorkbench.cpp' 'condition_drop::Status VariantWorkbench::ApplyConditionalActionDropTransaction('),
    (Extract-Function 'src/VariantWorkbench.Visibility.cpp' 'bool VariantWorkbench::SetConditionalVisibilityRuleTarget('),
    (Extract-Function 'src/VariantWorkbench.Visibility.cpp' 'bool VariantWorkbench::SetConditionalVisibilityRuleVisible('),
    (Extract-Function 'src/VariantWorkbench.cpp' 'bool VariantWorkbench::ResetAllRows(')
) -join "`n`n"
Write-Generated 'WorkbenchTransactions.production.inc' $workbench
$grid = Read-Source 'src/ui/Menu.GridInventory.cpp'
$gridStart = $grid.IndexOf('namespace sfs {', [StringComparison]::Ordinal)
if ($gridStart -lt 0) { throw 'Missing Grid namespace' }
Write-Generated 'GridCostume.production.inc' $grid.Substring($gridStart)

$events = Read-Source 'src/workbench/EquipmentRefreshEventSink.cpp'
$globalsStart = $events.IndexOf('std::mutex g_actorRefreshMutex;', [StringComparison]::Ordinal)
$globalsEnd = $events.IndexOf('[[nodiscard]]', $globalsStart)
if ($globalsStart -lt 0 -or $globalsEnd -le $globalsStart) { throw 'Missing event queue globals' }
$globals = @(
    $events.Substring($globalsStart, $globalsEnd - $globalsStart),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' '[[nodiscard]] bool IsGameReady()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' '[[nodiscard]] std::uint64_t CurrentEventRefreshGeneration()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' "[[nodiscard]] bool`nIsCurrentEventRefreshGeneration(")
) -join "`n`n"
Write-Generated 'EquipmentQueueGlobals.production.inc' $globals
$queue = @(
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' 'EquipmentRefreshEventSink *EquipmentRefreshEventSink::GetSingleton()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' 'void EquipmentRefreshEventSink::CancelQueuedRefreshes()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' 'void EquipmentRefreshEventSink::QueueRefresh()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' 'void EquipmentRefreshEventSink::RunRefresh()'),
    (Extract-Function 'src/workbench/EquipmentRefreshEventSink.cpp' 'void EquipmentRefreshEventSink::TickConditionState()')
) -join "`n`n"
Write-Generated 'EquipmentQueue.production.inc' $queue
