# Converts an ApeX N7SIX logo BMP (128x64, 1bpp, uncompressed) into the
# ST7565-native layout used by ui/welcome.c: 8 pages x 128 columns, column
# major, bit 0 = top pixel of the page, bit set = dark (ink) pixel.
# page 0 -> gStatusLine, pages 1..7 -> gFrameBuffer lines 0..6.
#
# Converts both variants:
#   archive/Apex_N7SIX_UVK1_128x64_monochrome.bmp          -> apex_logo.inc
#   archive/Apex_N7SIX_UVK1_128x64_monochrome_inverted.bmp -> apex_logo_inv.inc
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\misc\convert_apex_logo.ps1

$ErrorActionPreference = 'Stop'

function Convert-Logo([string]$bmpName, [string]$outName) {
    $bmpPath = Join-Path $PSScriptRoot "..\..\archive\$bmpName"
    $outPath = Join-Path $PSScriptRoot $outName

    $b = [System.IO.File]::ReadAllBytes((Resolve-Path $bmpPath).Path)

    if ([System.Text.Encoding]::ASCII.GetString($b[0..1]) -ne 'BM') { throw "Not a BMP file: $bmpName" }
    $off      = [BitConverter]::ToInt32($b, 10)
    $hdrSize  = [BitConverter]::ToInt32($b, 14)
    $w        = [BitConverter]::ToInt32($b, 18)
    $h        = [BitConverter]::ToInt32($b, 22)   # > 0 => bottom-up rows
    $planes   = [BitConverter]::ToInt16($b, 26)
    $bpp      = [BitConverter]::ToInt16($b, 28)
    $compress = [BitConverter]::ToInt32($b, 30)

    if ($w -ne 128 -or $h -ne 64 -or $planes -ne 1 -or $bpp -ne 1 -or $compress -ne 0) {
        throw "Unexpected BMP geometry in ${bmpName}: ${w}x${h} planes=$planes bpp=$bpp compression=$compress"
    }

    # Palette (2 x BGRA at offset 14 + hdrSize): the darker entry is the ink color.
    $palAt = 14 + $hdrSize
    $sum0 = [int]$b[$palAt] + $b[$palAt + 1] + $b[$palAt + 2]
    $sum1 = [int]$b[$palAt + 4] + $b[$palAt + 5] + $b[$palAt + 6]
    $inkIdx = if ($sum0 -le $sum1) { 0 } else { 1 }

    $stride = [Math]::Ceiling($w / 32.0) * 4   # 128 px / 8 = 16 B, already 4-aligned

    # Capture loop state in locals so the nested function sees this file's bytes.
    $fb = $b; $foff = $off; $fstride = $stride; $fink = $inkIdx; $fh = $h
    function Get-Pixel([int]$x, [int]$y) {   # y = top-down row
        $row = $fh - 1 - $y                    # file rows are bottom-up
        $byte = $fb[$foff + $row * $fstride + [int]([Math]::Floor($x / 8))]
        $bit  = ($byte -shr (7 - ($x % 8))) -band 1
        return ($bit -eq $fink)
    }

    # BMP -> ST7565 pages
    $out = New-Object byte[] 1024
    for ($p = 0; $p -lt 8; $p++) {
        for ($x = 0; $x -lt 128; $x++) {
            $v = 0
            for ($k = 0; $k -lt 8; $k++) {
                if (Get-Pixel $x ($p * 8 + $k)) { $v = $v -bor (1 -shl $k) }
            }
            $out[$p * 128 + $x] = [byte]$v
        }
    }

    # Round-trip: decode the ST7565 bytes back and compare with the BMP pixels.
    $mismatch = 0
    for ($y = 0; $y -lt 64; $y++) {
        for ($x = 0; $x -lt 128; $x++) {
            $page  = [int][Math]::Floor($y / 8)
            $bitv  = ($out[$page * 128 + $x] -shr ($y % 8)) -band 1
            if (($bitv -eq 1) -ne (Get-Pixel $x $y)) { $mismatch++ }
        }
    }
    if ($mismatch -ne 0) { throw "Round-trip failed for ${bmpName}: $mismatch mismatching pixels" }
    Write-Host "${bmpName}: round-trip OK (0 mismatches), ink palette index = $inkIdx"

    # C initializer, 16 bytes per line, one comment per LCD page (8 rows).
    $sb = New-Object System.Text.StringBuilder
    for ($p = 0; $p -lt 8; $p++) {
        [void]$sb.AppendLine("    // page $p (LCD rows $($p * 8)..$($p * 8 + 7))")
        for ($i = 0; $i -lt 8; $i++) {
            $vals = for ($j = 0; $j -lt 16; $j++) { '0x{0:X2}' -f $out[$p * 128 + $i * 16 + $j] }
            $comma = if ($p -eq 7 -and $i -eq 7) { '' } else { ',' }
            [void]$sb.AppendLine('    ' + ($vals -join ', ') + $comma)
        }
    }
    [System.IO.File]::WriteAllText($outPath, $sb.ToString(), [System.Text.Encoding]::ASCII)
    Write-Host "Wrote $([Math]::Round((Get-Item $outPath).Length / 1024.0, 1)) KB to $outPath"

    # ASCII preview (ink = '#'), top-down, to eyeball the artwork.
    for ($y = 0; $y -lt 64; $y += 2) {
        $line = ''
        for ($x = 0; $x -lt 128; $x++) { $line += $(if (Get-Pixel $x $y) { '#' } else { '.' }) }
        Write-Host $line
    }
    Write-Host ''
}

Convert-Logo 'Apex_N7SIX_UVK1_128x64_monochrome.bmp' 'apex_logo.inc'
Convert-Logo 'Apex_N7SIX_UVK1_128x64_monochrome_inverted.bmp' 'apex_logo_inv.inc'
