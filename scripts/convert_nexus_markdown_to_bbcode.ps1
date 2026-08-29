param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$utf8 = [System.Text.UTF8Encoding]::new($false)
$lines = [System.IO.File]::ReadAllLines((Resolve-Path $InputPath), $utf8)

# Release notes wrap long list items onto indented continuation lines. Join
# them before conversion so each Markdown item remains one valid BBCode item.
$normalizedLines = [System.Collections.Generic.List[string]]::new()
foreach ($line in $lines) {
    $continuation = [regex]::Match($line, '^\s{2,}(.+)$')
    if ($continuation.Success -and $normalizedLines.Count -gt 0 -and
        [regex]::IsMatch($normalizedLines[$normalizedLines.Count - 1], '^(?:[-*]|\d+\.) .+$')) {
        $lastIndex = $normalizedLines.Count - 1
        $mergedLine = $normalizedLines[$lastIndex] + ' ' + $continuation.Groups[1].Value.Trim()
        $normalizedLines.RemoveAt($lastIndex)
        $normalizedLines.Add($mergedLine)
        continue
    }
    $normalizedLines.Add($line)
}
$lines = $normalizedLines
$output = [System.Collections.Generic.List[string]]::new()
$listKind = $null

function Close-List {
    if ($script:listKind) {
        $script:output.Add('[/list]')
        $script:output.Add('')
        $script:listKind = $null
    }
}

function Convert-Inline([string]$text) {
    $text = [regex]::Replace($text, '<br\s*/?>', '[br]', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    $text = [regex]::Replace($text, '!\[([^\]]*)\]\(([^)]+)\)', '[center][img]$2[/img][/center]')
    $text = [regex]::Replace($text, '\[([^\]]+)\]\((https?://[^)]+)\)', '[url=$2]$1[/url]')
    $text = [regex]::Replace($text, '\*\*([^*]+)\*\*', '[b]$1[/b]')
    $text = [regex]::Replace($text, '`([^`]+)`', '[font=Courier New]$1[/font]')
    return $text
}

function Split-TableRow([string]$row) {
    $trimmed = $row.Trim()
    if ($trimmed.StartsWith('|')) { $trimmed = $trimmed.Substring(1) }
    if ($trimmed.EndsWith('|')) { $trimmed = $trimmed.Substring(0, $trimmed.Length - 1) }
    return @($trimmed.Split('|') | ForEach-Object { $_.Trim() })
}

$tableSeparatorPattern = '^\s*\|?\s*:?-{3,}:?\s*(?:\|\s*:?-{3,}:?\s*)+\|?\s*$'

for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]

    if (($i + 1) -lt $lines.Count -and $line.Contains('|') -and $lines[$i + 1] -match $tableSeparatorPattern) {
        Close-List
        $headers = Split-TableRow $line
        $output.Add('[table]')
        $output.Add('[tr]')
        foreach ($header in $headers) {
            $output.Add("[td][b]$(Convert-Inline $header)[/b][/td]")
        }
        $output.Add('[/tr]')

        $rowIndex = $i + 2
        while ($rowIndex -lt $lines.Count -and $lines[$rowIndex].Contains('|') -and $lines[$rowIndex].Trim().Length -gt 0) {
            $cells = Split-TableRow $lines[$rowIndex]
            $output.Add('[tr]')
            foreach ($cell in $cells) {
                $output.Add("[td]$(Convert-Inline $cell)[/td]")
            }
            $output.Add('[/tr]')
            $rowIndex++
        }
        $output.Add('[/table]')
        $output.Add('')
        $i = $rowIndex - 1
        continue
    }

    if ($line -match '^# (.+)$') {
        Close-List
        $output.Add('[center]')
        $output.Add("[size=6][b][color=#42ACC8]$(Convert-Inline $Matches[1])[/color][/b][/size]")
        $output.Add('[/center]')
        $output.Add('')
        continue
    }
    if ($line -match '^## (.+)$') {
        Close-List
        if ($output.Count -gt 0) { $output.Add('[line]') }
        $output.Add("[size=4][b][color=#42ACC8]$(Convert-Inline $Matches[1])[/color][/b][/size]")
        $output.Add('')
        continue
    }
    if ($line -match '^### (.+)$') {
        Close-List
        $output.Add("[size=3][b][color=#42ACC8]$(Convert-Inline $Matches[1])[/color][/b][/size]")
        $output.Add('')
        continue
    }
    if ($line -match '^[-*] (.+)$') {
        if ($listKind -ne 'unordered') {
            Close-List
            $output.Add('[list]')
            $listKind = 'unordered'
        }
        $output.Add("[*]$(Convert-Inline $Matches[1])[/*]")
        continue
    }
    if ($line -match '^\d+\. (.+)$') {
        if ($listKind -ne 'ordered') {
            Close-List
            $output.Add('[list=1]')
            $listKind = 'ordered'
        }
        $output.Add("[*]$(Convert-Inline $Matches[1])[/*]")
        continue
    }

    Close-List
    if ($line -match '^> (.+)$') {
        $output.Add("[quote]$(Convert-Inline $Matches[1])[/quote]")
    } else {
        $output.Add((Convert-Inline $line))
    }
}
Close-List

while ($output.Count -gt 0 -and $output[$output.Count - 1] -eq '') {
    $output.RemoveAt($output.Count - 1)
}
[System.IO.File]::WriteAllLines($OutputPath, $output, $utf8)
