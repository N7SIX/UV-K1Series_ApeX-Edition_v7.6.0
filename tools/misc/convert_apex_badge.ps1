# Converts "archive/ApeX Monochrome Badge.png" into the ST7565-native 128x64
# 1-bit layout used by BITMAP_ApexLogo / BITMAP_ApexLogoInv in App/bitmaps.c.
#
# The badge is a wide 32-bit RGBA PNG (aspect ~3.68); the screen is 128x64
# (aspect 2.0), so it is rescaled to fit. Dithering (Bayer 4x4 by default)
# approximates the grayscale edges on the 1-bit LCD.
#
# Outputs tools/misc/apex_badge.inc (normal) + apex_badge_inv.inc (inverted
# twin, exact bit inverse) and prints an ASCII preview for review. Nothing is
# written into App/bitmaps.c until you run the apply step.
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\misc\convert_apex_badge.ps1 [-Fit fit|crop] [-Dither bayer4|bayer8|floyd|none]

[CmdletBinding()]
param(
    [ValidateSet('fit', 'crop')][string]$Fit = 'fit',
    [ValidateSet('bayer4', 'bayer8', 'floyd', 'none')][string]$Dither = 'bayer4'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root     = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$srcPath  = Join-Path $root 'archive\ApeX Monochrome Badge.png'
$LCD_W = 128; $LCD_H = 64

# --- load source (flatten alpha onto white) --------------------------------
$srcImg = [System.Drawing.Image]::FromFile($srcPath)
$srcBmp = New-Object System.Drawing.Bitmap $LCD_W, $LCD_H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($srcBmp)
$g.Clear([System.Drawing.Color]::White)
$g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$sw = $srcImg.Width; $sh = $srcImg.Height
$scale = [Math]::Min($LCD_W / [double]$sw, $LCD_H / [double]$sh)   # contain
if ($Fit -eq 'crop') { $scale = [Math]::Max($LCD_W / [double]$sw, $LCD_H / [double]$sh) } # cover
$dw = [int][Math]::Round($sw * $scale)
$dh = [int][Math]::Round($sh * $scale)
$dx = [int][Math]::Floor(($LCD_W - $dw) / 2.0)
$dy = [int][Math]::Floor(($LCD_H - $dh) / 2.0)
$g.DrawImage($srcImg, (New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh), (New-Object System.Drawing.Rectangle 0, 0, $sw, $sh), ([System.Drawing.GraphicsUnit]::Pixel))
$g.Dispose(); $srcImg.Dispose()

# --- build coverage grid (0..1 ink) from the 24bpp render ------------------
$cov = New-Object 'double[,]' $LCD_W, $LCD_H
for ($y = 0; $y -lt $LCD_H; $y++) {
    for ($x = 0; $x -lt $LCD_W; $x++) {
        $c = $srcBmp.GetPixel($x, $y)
        $lum = (0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B) / 255.0
        $cov[$x, $y] = 1.0 - $lum
    }
}
$srcBmp.Dispose()

# --- dither to 1-bit --------------------------------------------------------
function Get-Bayer4() { return ,(@(0,8,2,10; 12,4,14,6; 3,11,1,9; 15,7,13,5)) }
function Get-Bayer8() {
    # jagged array (array of rows); row = y, element = x
    $b = @(0,8,2,10; 12,4,14,6; 3,11,1,9; 15,7,13,5)
    $m = New-Object 'int[][]' 8
    for ($i = 0; $i -lt 8; $i++) { $m[$i] = New-Object 'int[]' 8 }
    for ($yy = 0; $yy -lt 4; $yy++) {
        for ($xx = 0; $xx -lt 4; $xx++) {
            $v = $b[$yy][$xx] * 4
            $m[$yy][$xx]         = $v + 0
            $m[$yy][$xx + 4]     = $v + 2
            $m[$yy + 4][$xx]     = $v + 3
            $m[$yy + 4][$xx + 4] = $v + 1
        }
    }
    return ,$m
}

$ink = New-Object 'bool[,]' $LCD_W, $LCD_H
switch ($Dither) {
    'none'    { for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $ink[$x,$y] = ($cov[$x,$y] -ge 0.5) }} }
    'bayer4'  { $m = Get-Bayer4; for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $mi = $x % 4; $mj = $y % 4; $mv = $m[$mj][$mi]; $t = ($mv + 0.5)/16.0; $ink[$x,$y] = ($cov[$x,$y] -gt $t) }} }
    'bayer8'  { $m = Get-Bayer8; for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $mi = $x % 8; $mj = $y % 8; $mv = $m[$mj][$mi]; $t = ($mv + 0.5)/64.0; $ink[$x,$y] = ($cov[$x,$y] -gt $t) }} }
    'floyd'   {
        $buf = New-Object 'double[,]' $LCD_W, $LCD_H
        for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $buf[$x,$y] = $cov[$x,$y] }}
        for ($y=0;$y -lt $LCD_H;$y++){ for ($x=0;$x -lt $LCD_W;$x++){
            $old = $buf[$x,$y]; $nv = 0.0; if ($old -ge 0.5){$nv = 1.0}
            $ink[$x,$y] = ($nv -ge 0.5); $err = $old - $nv
            if (($x+1) -lt $LCD_W){ $buf[$x+1,$y] += $err*0.4375 }
            if ((($x-1) -ge 0) -and (($y+1) -lt $LCD_H)){ $buf[$x-1,$y+1] += $err*0.1875 }
            if (($y+1) -lt $LCD_H){ $buf[$x,$y+1] += $err*0.3125 }
            if ((($x+1) -lt $LCD_W) -and (($y+1) -lt $LCD_H)){ $buf[$x+1,$y+1] += $err*0.0625 }
        } }
    }
}

# --- pack ST7565 (bit set = ink) --------------------------------------------
$out = New-Object byte[] 1024
for ($p = 0; $p -lt 8; $p++) {
    for ($x = 0; $x -lt $LCD_W; $x++) {
        $v = 0
        for ($k = 0; $k -lt 8; $k++) { $yy = ($p * 8) + $k; if ($ink[$x, $yy]) { $v = $v -bor (1 -shl $k) } }
        $out[($p * 128) + $x] = [byte]$v
    }
}

# --- emit .inc (16 bytes/line, per-page comments) ---------------------------
function Emit-Inc([byte[]]$bytes, [string]$outPath) {
    $sb = New-Object System.Text.StringBuilder
    for ($p = 0; $p -lt 8; $p++) {
        [void]$sb.AppendLine("    // page $p (LCD rows $($p*8)..$($p*8+7))")
        for ($i = 0; $i -lt 8; $i++) {
            $vals = for ($j = 0; $j -lt 16; $j++) { '0x{0:X2}' -f $bytes[($p*128) + ($i*16) + $j] }
            $comma = if ($p -eq 7 -and $i -eq 7) { '' } else { ',' }
            [void]$sb.AppendLine('    ' + ($vals -join ', ') + $comma)
        }
    }
    [System.IO.File]::WriteAllText($outPath, $sb.ToString(), [System.Text.Encoding]::ASCII)
    Write-Host ("Wrote {0} ({1} bytes)" -f (Split-Path $outPath -Leaf), $bytes.Length)
}
Emit-Inc $out (Join-Path $PSScriptRoot 'apex_badge.inc')

$inv = New-Object byte[] 1024
for ($i = 0; $i -lt 1024; $i++) { $inv[$i] = $out[$i] -bxor 0xFF }
Emit-Inc $inv (Join-Path $PSScriptRoot 'apex_badge_inv.inc')

# --- ASCII preview (ink = '#') ---------------------------------------------
$n = 0
for ($y = 0; $y -lt $LCD_H; $y++) { for ($x = 0; $x -lt $LCD_W; $x++) { if ($ink[$x, $y]) { $n++ } } }
Write-Host ("`nFit=$Fit  Dither=$Dither  source ${sw}x${sh} -> ${LCD_W}x${LCD_H}  ink $n / 8192 ({0:N1}%)" -f (100.0*$n/8192.0))
Write-Host ('+' + ('-' * $LCD_W) + '+')
for ($y = 0; $y -lt $LCD_H; $y++) {
    $sb2 = New-Object System.Text.StringBuilder ($LCD_W + 2)
    [void]$sb2.Append('|')
    for ($x = 0; $x -lt $LCD_W; $x++) { if ($ink[$x, $y]) { [void]$sb2.Append('#') } else { [void]$sb2.Append('.') } }
    [void]$sb2.Append('|')
    Write-Host $sb2.ToString()
}
Write-Host ('+' + ('-' * $LCD_W) + '+')

