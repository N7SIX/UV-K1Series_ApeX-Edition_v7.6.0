# Applies tools/misc/apex_badge.inc (+ apex_badge_inv.inc) into the
# BITMAP_ApexLogo / BITMAP_ApexLogoInv array bodies in App/bitmaps.c.
#
# - Backs up App/bitmaps.c to App/bitmaps.c.bak-<timestamp> before editing.
# - Verifies each .inc has exactly 1024 bytes and the inverted one is the exact
#   bitwise inverse of the normal one before writing.
# - Verifies after editing that both arrays still hold exactly 1024 bytes and
#   that ApexLogoInv == ApexLogo ^ 0xFF byte-for-byte.
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\misc\apply_apex_badge.ps1

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$root = Resolve-Path (Join-Path $here '..\..')
$bmC  = Join-Path $root 'App\bitmaps.c'
$incN = Join-Path $here 'apex_badge.inc'
$incI = Join-Path $here 'apex_badge_inv.inc'

function Read-Bytes([string]$path) {
    $t = [IO.File]::ReadAllText($path)
    $b = @([regex]::Matches($t, '0x([0-9A-Fa-f]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
    return ,$b
}

$nB = Read-Bytes $incN
$iB = Read-Bytes $incI
if ($nB.Count -ne 1024) { throw "apex_badge.inc has $($nB.Count) bytes, expected 1024" }
if ($iB.Count -ne 1024) { throw "apex_badge_inv.inc has $($iB.Count) bytes, expected 1024" }
$bad = 0
for ($i = 0; $i -lt 1024; $i++) { if ($iB[$i] -ne ($nB[$i] -bxor 0xFF)) { $bad++ } }
if ($bad -ne 0) { throw "inverted .inc is not exact inverse of normal ($bad bytes differ)" }
Write-Host 'Verified: both .inc are 1024 bytes and inverted is exact bitwise inverse.'

# Build replacement initializer text (with page comments), indented to match.
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
$invBody    = Build-Body $iB

$src = [IO.File]::ReadAllText($bmC)

# Replace each array body between the '{' after the declaration and its closing '};'
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
$updated = Replace-Array $updated 'BITMAP_ApexLogoInv' $invBody

# --- verify the edit in-memory before touching disk ------------------------
function Array-Bytes([string]$text, [string]$symbol) {
    $s = $text.IndexOf("const uint8_t $symbol[1024] =")
    $b = $text.IndexOf('{', $s); $e = $text.IndexOf('};', $b)
    $body = $text.Substring($b, $e - $b)
    return @([regex]::Matches($body, '0x([0-9A-Fa-f]{2})') | ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
}
$vn = Array-Bytes $updated 'BITMAP_ApexLogo'
$vi = Array-Bytes $updated 'BITMAP_ApexLogoInv'
if ($vn.Count -ne 1024) { throw "post-edit BITMAP_ApexLogo has $($vn.Count) bytes" }
if ($vi.Count -ne 1024) { throw "post-edit BITMAP_ApexLogoInv has $($vi.Count) bytes" }
$bad2 = 0
for ($i = 0; $i -lt 1024; $i++) { if ($vi[$i] -ne ($vn[$i] -bxor 0xFF)) { $bad2++ } }
if ($bad2 -ne 0) { throw "post-edit inverse check failed ($bad2 bytes)" }
$matchesN = ($vn -join ',') -eq ($nB -join ',')
if (-not $matchesN) { throw "post-edit BITMAP_ApexLogo does not match apex_badge.inc" }
Write-Host 'Verified: bitmaps.c will contain 1024-byte arrays, exact inverse, matching the .inc.'

# --- backup + write --------------------------------------------------------
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$bak = "$bmC.bak-$stamp"
Copy-Item $bmC $bak
Write-Host "Backup: $bak"
[IO.File]::WriteAllText($bmC, $updated)
Write-Host "Updated: $bmC (BITMAP_ApexLogo + BITMAP_ApexLogoInv replaced with the badge)"
