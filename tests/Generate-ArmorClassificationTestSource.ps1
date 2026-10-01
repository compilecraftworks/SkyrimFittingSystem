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
            if ($depth -eq 0) {
                $end = $i+1
                if ($source[$end] -eq ';') { ++$end }
                return $source.Substring($start, $end-$start)
            }
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
$groups = @{
    'ClassificationTypes.production.inc' = @('src/native/ArmorSkinning.h', @(
        'enum class ArmorGenitalKeywordDisposition', 'struct ArmorGenitalKeywordOverride'))
    'Classification.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'enum class SOSUserArmorPolicy',
        '[[nodiscard]] bool IsAsciiAlpha(', '[[nodiscard]] bool IsAsciiUpper(',
        '[[nodiscard]] bool IsAsciiLower(', '[[nodiscard]] bool IsAsciiDigit(',
        '[[nodiscard]] bool IsAsciiAlphaNumeric(',
        '[[nodiscard]] std::string BuildSearchTextPart(', 'void AppendSearchText(',
        '[[nodiscard]] bool ContainsSearchTerm(', 'template <std::size_t N>',
        ('[[nodiscard]] std::string' + "`n" + 'BuildArmorClassificationText('),
        '[[nodiscard]] std::uint32_t BodySlotMask()',
        '[[nodiscard]] std::uint32_t PelvisPrimarySlotMask()',
        ('[[nodiscard]] std::uint32_t' + "`n" + 'GetDisplaySlotMask('),
        'struct RuntimeKeywords', '[[nodiscard]] RuntimeKeywords LookupRuntimeKeywords()',
        ('[[nodiscard]] std::uint64_t' + "`n" + 'RuntimeKeywordOwnershipKey('),
        'void SynchronizeSFSOwnedRuntimeKeyword(',
        ('[[nodiscard]] SOSUserArmorPolicy' + "`n" + 'GetSOSUserArmorPolicy('),
        ('[[nodiscard]] bool' + "`n" + 'IsLikelyGenitalConcealingPelvisArmor('),
        ('[[nodiscard]] bool' + "`n" + 'IsLikelyUpperOnlyBodyArmor('),
        ('[[nodiscard]] ArmorGenitalKeywordOverride' + "`n" + 'ResolveArmorGenitalKeywordOverride(const RE::TESObjectARMO *a_armor) {'),
        'void SynchronizeArmorClassificationKeywordsImpl('))
    'ClassificationPublic.production.inc' = @('src/native/ArmorSkinning.cpp', @(
        'void SynchronizeArmorClassificationKeywords(',
        ('ArmorGenitalKeywordOverride' + "`n" + 'GetArmorGenitalKeywordOverride(')))
    'ClassificationTokens.production.inc' = @('src/features/virtual_tokens/VirtualWornTokens.cpp', @(
        'enum class SosTngClassificationKeyword',
        ('[[nodiscard]] SosTngClassificationKeyword' + "`n" + 'ClassifySosTngClassificationKeyword('),
        'void AppendKeywordByEditorID(',
        ('[[nodiscard]] std::vector<RE::BGSKeyword *>' + "`n" + 'BuildEffectiveSourceKeywords(')))
}
foreach ($entry in $groups.GetEnumerator()) {
    Emit $entry.Key (($entry.Value[1] | ForEach-Object { Extract $entry.Value[0] $_ }) -join "`n`n")
}
Emit 'GenitalEnvironment.production.inc' ([regex]::Replace(
    (Read-Source 'src/native/GenitalCompatibility.cpp'), '(?m)^#include "[^"\n]+"\n', ''))
