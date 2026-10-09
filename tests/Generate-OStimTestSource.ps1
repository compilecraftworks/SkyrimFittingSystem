param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/features/virtual_tokens/VirtualWornTokens.cpp')).Replace("`r`n", "`n")
function Extract([string]$Signature) {
    $start = $source.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing $Signature" }
    $open = $source.IndexOf('{', $start)
    $depth = 0
    for ($i = $open; $i -lt $source.Length; ++$i) {
        if ($source[$i] -eq '{') { ++$depth }
        if ($source[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                $suffix = if ($Signature.StartsWith('struct ')) { ';' } else { '' }
                return $source.Substring($start, $i+1-$start) + $suffix
            }
        }
    }
    throw "Unbalanced $Signature"
}
$bodies = @('struct RegisteredAppearance {', 'struct AppearanceTicketRef {', 'struct SuppressionTicket {', 'struct CallerIdentity {', 'struct StackObservation {') | ForEach-Object { Extract $_ }
$bodies += @'
std::mutex g_cacheMutex, g_runtimeMutex;
std::unordered_map<RE::FormID, std::vector<RegisteredAppearance>> g_registeredAppearances;
std::vector<SuppressionTicket> g_suppressionTickets;
std::unordered_map<RE::FormID, std::uint64_t> g_actorManualGenerations;
std::uint64_t g_nextTransactionID = 1;
std::unordered_map<std::uint32_t, StackObservation> g_stackObservations;
void PruneRuntimeStateLocked(RuntimeClock::time_point) {} // Host boundary only.
'@
$bodies += Extract 'void BeginOStimEquipmentPass('
$bodies += Extract 'void EndOStimEquipmentPass('
$bodies += Extract "[[nodiscard]] std::vector<RegisteredAppearance>`nGetRegisteredAppearances(const RE::FormID"
$bodies += Extract '[[nodiscard]] std::uint64_t AddTicket('
$start = $source.IndexOf('namespace {', $source.IndexOf('void ObserveRegisteredAppearanceWig('))
$end = $source.IndexOf('void ResetVirtualWornTokenRuntimeState() {', $start)
if ($start -lt 0 -or $end -lt 0) { throw 'OStim production slice missing' }
$bodies += $source.Substring($start, $end - $start)
$text = $bodies -join "`n`n"
$absolute = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory((Split-Path -Parent $absolute)) | Out-Null
if (-not [IO.File]::Exists($absolute) -or [IO.File]::ReadAllText($absolute) -cne $text) {
    [IO.File]::WriteAllText($absolute, $text, [Text.UTF8Encoding]::new($false))
}
