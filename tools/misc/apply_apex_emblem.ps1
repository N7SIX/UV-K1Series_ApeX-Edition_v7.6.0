# Applies tools/misc/apex_emblem.inc into the BITMAP_ApexLogo array body in
# App/bitmaps.c (the ApeX boot splash), and updates the leading source comment
# to reference the emblem PNG.
#
# - Backs up App/bitmaps.c to App/bitmaps.c.bak-<timestamp> before editing.
# - Verifies the .inc has exactly 1024 bytes before writing.
# - Verifies after editing that BITMAP_ApexLogo still holds exactly 1024 bytes
#   and matches the .inc byte-for-byte.
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\misc\apply_apex_emblem.ps1

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$root = Resolve-Path (Join-Path $here '..\..')
$bmC  = Join-Path $root 'App\bitmaps.c'
$inc  = Join-Path $here 'apex_emblem.inc'

function Read-Bytes([string]$path) {
    $t = [IO.File]::ReadAllText($path)
    $b = @([regex]::Matches($t, '0x([0-9A-Fa-f]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
    return ,$b
}

$nB = Read-Bytes $inc
if ($nB.Count -ne 1024) { throw "apex_emblem.inc has $($nB.Count) bytes, expected 1024" }
Write-Host 'Verified: apex_emblem.inc has 1024 bytes.'

# Build the replacement initializer body (page comments, 16 bytes/line).
function Build-Body([byte[]]$bytes) {
    $sb = New-Object System.Text.StringBuilder
    for ($p = 0; $p -lt 8; $p++) {
        [void]$sb.AppendLine("    // page $p (LCD rows $($p*8)..$($p*8+7))")
        for ($i = 0; $i -lt 8; $i++) {
            $vals = for ($j = 0; $j -lt 16; $j++) { '0x{0:X2}' -f $bytes[($p*128) + ($i*16) + $j] }
            $comma = if ($p -eq 7 -and $i -eq 7) { '' } else { ',' }
            [void]$sb.AppendLine('    ' + ($vals -join ', ') + $comma)
        }
    }
    return $sb.ToString().TrimEnd("`r","`n")
}
$normalBody = Build-Body $nB

$src = [IO.File]::ReadAllText($bmC)

# Replace the array body between the '{' after the declaration and its '};'.
function Replace-Array([string]$text, [string]$symbol, [string]$newBody) {
    $start = $text.IndexOf("const uint8_t $symbol[1024] =")
    if ($start -lt 0) { throw "symbol $symbol not found in bitmaps.c" }
    $brace = $text.IndexOf('{', $start)
    $end = $text.IndexOf('};', $brace)
    if ($brace -lt 0 -or $end -lt 0) { throw "could not locate body of $symbol" }
    $before = $text.Substring(0, $brace + 1)
    $after = $text.Substring($end)
    return $before + "`r`n" + $newBody + "`r`n" + $after
}
$updated = Replace-Array $src 'BITMAP_ApexLogo' $normalBody

# --- update the leading source comment to point at the emblem PNG -----------
$oldComment = @'
// ApeX Edition logo: boot splash shown for ~300 ms before the welcome screen
// (see Main() in main.c), baked into the firmware so it needs no boot-logo
// upload. Source: archive/ApeX Monochrome Badge.png, rescaled to fit 128x64
// and converted to the ST7565-native layout by tools/misc/convert_apex_badge.ps1:
'@
$newComment = @'
// ApeX Edition logo: boot splash shown for ~300 ms before the welcome screen
// (see Main() in main.c), baked into the firmware so it needs no boot-logo
// upload. Source: archive/ApeX Emblem_N7SIX.png, auto-cropped to its inked
// content, fit-to-width and vertically centered, then converted to the
// ST7565-native layout by tools/misc/convert_apex_emblem.ps1:
'@
if ($updated.Contains($oldComment)) {
    $updated = $updated.Replace($oldComment, $newComment)
} else {
    # fall back: just rewrite the source-file line wherever it appears
    $updated = $updated -replace 'Source: archive/ApeX Monochrome Badge\.png', 'Source: archive/ApeX Emblem_N7SIX.png'
    $updated = $updated -replace 'tools/misc/convert_apex_badge\.ps1', 'tools/misc/convert_apex_emblem.ps1'
}

# --- verify the edit in-memory before touching disk -------------------------
function Array-Bytes([string]$text, [string]$symbol) {
    $s = $text.IndexOf("const uint8_t $symbol[1024] =")
    $b = $text.IndexOf('{', $s); $e = $text.IndexOf('};', $b)
    $body = $text.Substring($b, $e - $b)
    return @([regex]::Matches($body, '0x([0-9A-Fa-f]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
}
$vn = Array-Bytes $updated 'BITMAP_ApexLogo'
if ($vn.Count -ne 1024) { throw "post-edit BITMAP_ApexLogo has $($vn.Count) bytes" }
if (($vn -join ',') -ne ($nB -join ',')) { throw "post-edit BITMAP_ApexLogo does not match apex_emblem.inc" }
Write-Host 'Verified: bitmaps.c will contain a 1024-byte BITMAP_ApexLogo matching the emblem .inc.'

# --- backup + write ---------------------------------------------------------
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$bak = "$bmC.bak-$stamp"
Copy-Item $bmC $bak
Write-Host "Backup: $bak"
[IO.File]::WriteAllText($bmC, $updated)
Write-Host "Updated: $bmC (BITMAP_ApexLogo replaced with the ApeX Emblem_N7SIX artwork)"
