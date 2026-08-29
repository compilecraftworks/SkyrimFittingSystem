param(
    [Parameter(Mandatory = $true)] [string]$InputPath,
    [Parameter(Mandatory = $true)] [string]$OutputPath
)

$utf8 = [System.Text.UTF8Encoding]::new($false)
$lines = [System.IO.File]::ReadAllLines((Resolve-Path $InputPath), $utf8)

# Markdown release notes wrap long list items onto indented continuation lines.
# Join those continuations before rendering so an HTML list item remains one item.
$normalizedLines = [System.Collections.Generic.List[string]]::new()
foreach ($line in $lines) {
    $continuation = [regex]::Match($line, '^\s{2,}(.+)$')
    if ($continuation.Success -and $normalizedLines.Count -gt 0 -and
        [regex]::IsMatch($normalizedLines[$normalizedLines.Count - 1], '^[-*] .+$')) {
        $lastIndex = $normalizedLines.Count - 1
        $mergedLine = $normalizedLines[$lastIndex] + ' ' + $continuation.Groups[1].Value.Trim()
        $normalizedLines.RemoveAt($lastIndex)
        $normalizedLines.Add($mergedLine)
        continue
    }
    $normalizedLines.Add($line)
}
$lines = $normalizedLines
$imageUrls = @{
    'NEXUS_SCREENSHOT_01_URL' = 'arca-assets/v1.2.2/01_actor_selection.png'
    'NEXUS_SCREENSHOT_02_URL' = 'arca-assets/v1.2.2/02_workbench_layout.png'
    'NEXUS_SCREENSHOT_03_URL' = 'arca-assets/v1.2.2/03_equipment_register.png'
    'NEXUS_SCREENSHOT_04_URL' = 'arca-assets/v1.2.2/04_visibility_controls.png'
    'NEXUS_SCREENSHOT_05_URL' = 'arca-assets/v1.2.2/05_condition_settings.png'
    'NEXUS_SCREENSHOT_06_URL' = 'arca-assets/v1.2.2/06_action_settings.png'
    'NEXUS_SCREENSHOT_07_URL' = 'arca-assets/v1.2.2/07_fitting_kit.png'
    'NEXUS_SCREENSHOT_08_URL' = 'arca-assets/v1.2.2/08_options_integrations.png'
}

function Convert-Inline([string]$text) {
    $text = [System.Net.WebUtility]::HtmlEncode($text)
    $text = [regex]::Replace($text, '!\[([^\]]*)\]\(([^)]+)\)', {
        param($match)
        $alt = $match.Groups[1].Value
        $src = $match.Groups[2].Value
        if ($imageUrls.ContainsKey($src)) { $src = $imageUrls[$src] }
        return "<figure><a href=`"$src`" target=`"_blank`" rel=`"noopener`"><img src=`"$src`" alt=`"$alt`"></a><figcaption>$alt</figcaption></figure>"
    })
    $text = [regex]::Replace($text, '\[([^\]]+)\]\((https?://[^)]+)\)', '<a href="$2" target="_blank" rel="noopener">$1</a>')
    $text = [regex]::Replace($text, '\*\*([^*]+)\*\*', '<strong>$1</strong>')
    $text = [regex]::Replace($text, '`([^`]+)`', '<code>$1</code>')
    return $text
}

$body = [System.Collections.Generic.List[string]]::new()
$listKind = $null
function Close-List {
    if ($script:listKind -eq 'ul') { $script:body.Add('</ul>') }
    elseif ($script:listKind -eq 'ol') { $script:body.Add('</ol>') }
    $script:listKind = $null
}

foreach ($line in $lines) {
    if ($line -match '^# (.+)$') { Close-List; $body.Add("<h1>$(Convert-Inline $Matches[1])</h1>"); continue }
    if ($line -match '^## (.+)$') { Close-List; $body.Add('<hr>'); $body.Add("<h2>$(Convert-Inline $Matches[1])</h2>"); continue }
    if ($line -match '^### (.+)$') { Close-List; $body.Add("<h3>$(Convert-Inline $Matches[1])</h3>"); continue }
    if ($line -match '^[-*] (.+)$') {
        if ($listKind -ne 'ul') { Close-List; $body.Add('<ul>'); $listKind = 'ul' }
        $body.Add("<li>$(Convert-Inline $Matches[1])</li>")
        continue
    }
    if ($line -match '^\d+\. (.+)$') {
        if ($listKind -ne 'ol') { Close-List; $body.Add('<ol>'); $listKind = 'ol' }
        $body.Add("<li>$(Convert-Inline $Matches[1])</li>")
        continue
    }
    Close-List
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -match '^> (.+)$') { $body.Add("<blockquote>$(Convert-Inline $Matches[1])</blockquote>"); continue }
    $converted = Convert-Inline $line
    if ($converted.StartsWith('<figure>')) { $body.Add($converted) }
    else { $body.Add("<p>$converted</p>") }
}
Close-List

$html = @"
<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Skyrim Fitting System - 아카라이브 게시용</title>
<style>
body{max-width:980px;margin:32px auto;padding:0 24px;background:#202124;color:#e8eaed;font-family:Arial,"Noto Sans KR",sans-serif;line-height:1.72}
h1,h2,h3{color:#42acc8;line-height:1.35} h1{text-align:center;font-size:2.2rem} h2{font-size:1.55rem} h3{font-size:1.2rem}
hr{border:0;border-top:1px solid #4b4d50;margin:32px 0 20px} a{color:#63d3ea} code{background:#303236;padding:2px 5px;border-radius:4px}
figure{text-align:center;margin:24px auto} figure img{display:block;width:75%;height:auto;margin:auto;border-radius:5px} figcaption{margin-top:7px;color:#aeb4ba;font-size:.9rem}
blockquote{border-left:4px solid #42acc8;margin:20px 0;padding:10px 16px;background:#292b2f} li{margin:4px 0}
</style>
</head>
<body>
$($body -join "`r`n")
</body>
</html>
"@
[System.IO.File]::WriteAllText($OutputPath, $html.TrimEnd() + "`r`n", $utf8)
