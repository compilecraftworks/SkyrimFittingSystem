param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = (Get-Content -LiteralPath (Join-Path $repo 'src/native/RaceMenuBodyMorph.cpp') -Raw).Replace("`r`n", "`n")
$names = @(
  'struct RegisteredAppearanceNode {',
  '[[nodiscard]] bool IsRegisteredAppearanceDisplayActive(',
  ('[[nodiscard]] std::vector<RE::NiPointer<RE::NiAVObject>>' + "`n" + 'ResolveRegisteredAppearanceNodes('),
  '[[nodiscard]] bool ContainsExtraData(',
  'void RememberAndMorphNewNodes(',
  'class RegisteredAppearanceAttachmentObserver final',
  'void SetRegisteredAppearanceDisplayActive(',
  '[[nodiscard]] std::size_t ApplyMorphsToRegisteredAppearanceNodes(',
  'void QueuePendingMorphSync(',
  'void RecordMorphUpdateRequest(',
  'void QueueUpdateModelWeightAppearanceSync(',
  'void ApplyBodyMorphsHook(',
  'void ForgetRegisteredAppearanceNodes('
)
$blocks = [Collections.Generic.List[string]]::new()
foreach ($name in $names) {
  $start = $source.IndexOf($name, [StringComparison]::Ordinal)
  if ($start -lt 0) { throw "Missing production definition: $name" }
  $open = $source.IndexOf('{', $start)
  $semicolon = $source.IndexOf(';', $start)
  if ($semicolon -ge 0 -and $semicolon -lt $open) {
    $start = $source.IndexOf($name, $semicolon + 1, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing definition after declaration: $name" }
    $open = $source.IndexOf('{', $start)
  }
  $depth = 0
  $end = -1
  for ($i = $open; $i -lt $source.Length; $i++) {
    if ($source[$i] -eq '{') { $depth++ }
    if ($source[$i] -eq '}') {
      $depth--
      if ($depth -eq 0) { $end = $i + 1; break }
    }
  }
  if ($end -lt 0) { throw "Unbalanced production definition: $name" }
  if ($source[$end] -eq ';') { $end++ }
  $blocks.Add($source.Substring($start, $end - $start))
  if ($name.StartsWith('struct RegisteredAppearanceNode')) {
    $blocks.Add('std::unordered_map<RE::FormID, std::vector<RegisteredAppearanceNode>> g_registeredAppearanceNodes;')
  }
}
# Generated test input only. Never modifies or compiles the full plugin in this
# harness. Fail closed on source extraction/compile/assertion changes.
$content = $blocks -join "`n`n"
$target = [IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
if (-not [IO.File]::Exists($target) -or [IO.File]::ReadAllText($target) -cne $content) {
  [IO.File]::WriteAllText($target, $content, [Text.UTF8Encoding]::new($false))
}
