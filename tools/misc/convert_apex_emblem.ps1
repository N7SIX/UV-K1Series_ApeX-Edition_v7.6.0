# Converts "archive/ApeX Emblem_N7SIX.png" into the ST7565-native 128x64
# 1-bit layout used by BITMAP_ApexLogo (the ApeX boot splash) in App/bitmaps.c.
#
# The emblem PNG (1280x640, aspect 2.0) has generous padding; the real artwork
# (emblem + "APEX" + N7SIX text) is a wide horizontal band (aspect ~4.2). We
# auto-crop to the inked content, then fit-to-width and vertically center it so
# the logo is as large and legible as the 128x64 screen allows. Dithering
# (Bayer 4x4 by default) approximates the grayscale edges on the 1-bit LCD.
#
# Output: tools/misc/apex_emblem.inc + an ASCII preview for review.
# Nothing is written into App/bitmaps.c until you run apply_apex_emblem.ps1.
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\misc\convert_apex_emblem.ps1 [-Pad N] [-Dither bayer4|bayer8|floyd|none] [-NoCrop]

[CmdletBinding()]
param(
    [int]$Pad = 4,
    [ValidateSet('bayer4', 'bayer8', 'floyd', 'none')][string]$Dither = 'bayer4',
    [switch]$NoCrop
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root    = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$srcPath = Join-Path $root 'archive\ApeX Emblem_N7SIX.png'
$LCD_W = 128; $LCD_H = 64

# --- load source ------------------------------------------------------------
$srcImg = [System.Drawing.Image]::FromFile($srcPath)
$srcBmp = New-Object System.Drawing.Bitmap $srcImg
$sw = $srcBmp.Width; $sh = $srcBmp.Height

# --- detect content bbox (dark pixels) --------------------------------------
$minX = $sw; $minY = $sh; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $sh; $y++) {
    for ($x = 0; $x -lt $sw; $x++) {
        $c = $srcBmp.GetPixel($x, $y)
        $lum = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
        if ($lum -lt 200) {
            if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
if ($maxX -lt 0) { throw "No ink found in $srcPath" }

$cx0 = [Math]::Max(0, $minX - $Pad); $cy0 = [Math]::Max(0, $minY - $Pad)
$cx1 = [Math]::Min($sw - 1, $maxX + $Pad); $cy1 = [Math]::Min($sh - 1, $maxY + $Pad)
$cw = $cx1 - $cx0 + 1; $ch = $cy1 - $cy0 + 1
$srcRect = New-Object System.Drawing.Rectangle $cx0, $cy0, $cw, $ch
if ($NoCrop) { $srcRect = New-Object System.Drawing.Rectangle 0, 0, $sw, $sh; $cw = $sw; $ch = $sh }
Write-Host ("source ${sw}x${sh}  content ${cw}x${ch} (aspect {0:N2})" -f ($cw / [double]$ch))

# --- render cropped -> 128x64 white canvas, fit-to-width, vertically centered
$dst = New-Object System.Drawing.Bitmap $LCD_W, $LCD_H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.Clear([System.Drawing.Color]::White)
$g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$scale = [Math]::Min($LCD_W / [double]$cw, $LCD_H / [double]$ch)   # contain
$dw = [int][Math]::Round($cw * $scale)
$dh = [int][Math]::Round($ch * $scale)
$dx = [int][Math]::Floor(($LCD_W - $dw) / 2.0)
$dy = [int][Math]::Floor(($LCD_H - $dh) / 2.0)
$g.DrawImage($srcBmp, (New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh), $srcRect, ([System.Drawing.GraphicsUnit]::Pixel))
$g.Dispose()
Write-Host ("placed ${dw}x${dh} at (${dx},${dy})")

# --- coverage grid (0..1 ink) -----------------------------------------------
$cov = New-Object 'double[,]' $LCD_W, $LCD_H
for ($y = 0; $y -lt $LCD_H; $y++) {
    for ($x = 0; $x -lt $LCD_W; $x++) {
        $c = $dst.GetPixel($x, $y)
        $lum = (0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B) / 255.0
        $cov[$x, $y] = 1.0 - $lum
    }
}
$dst.Dispose(); $srcBmp.Dispose(); $srcImg.Dispose()
# --- dither to 1-bit --------------------------------------------------------
function Get-Bayer4() { return ,(@(0,8,2,10; 12,4,14,6; 3,11,1,9; 15,7,13,5)) }
$ink = New-Object 'bool[,]' $LCD_W, $LCD_H
switch ($Dither) {
    'none'   { for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $ink[$x,$y] = ($cov[$x,$y] -ge 0.5) }} }
    'bayer4' { $m = Get-Bayer4; for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $mi=$x % 4; $mj=$y % 4; $mv=$m[$mj][$mi]; $t=($mv+0.5)/16.0; $ink[$x,$y] = ($cov[$x,$y] -gt $t) }} }
    'bayer8' {
        $b4 = Get-Bayer4; $m8 = New-Object 'int[][]' 8
        for ($i=0;$i -lt 8;$i++){ $m8[$i]=New-Object 'int[]' 8 }
        for ($yy=0;$yy -lt 4;$yy++){for($xx=0;$xx -lt 4;$xx++){ $v=$b4[$yy][$xx]*4; $m8[$yy][$xx]=$v+0; $m8[$yy][$xx+4]=$v+2; $m8[$yy+4][$xx]=$v+3; $m8[$yy+4][$xx+4]=$v+1 }}
        for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $mi=$x % 8; $mj=$y % 8; $mv=$m8[$mj][$mi]; $t=($mv+0.5)/64.0; $ink[$x,$y] = ($cov[$x,$y] -gt $t) }}
    }
    'floyd'  {
        $buf = New-Object 'double[,]' $LCD_W, $LCD_H
        for ($y=0;$y -lt $LCD_H;$y++){for($x=0;$x -lt $LCD_W;$x++){ $buf[$x,$y] = $cov[$x,$y] }}
        for ($y=0;$y -lt $LCD_H;$y++){ for ($x=0;$x -lt $LCD_W;$x++){
            $old=$buf[$x,$y]; $nv=0.0; if($old -ge 0.5){$nv=1.0}; $ink[$x,$y]=($nv -ge 0.5); $err=$old-$nv
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

# --- emit .inc --------------------------------------------------------------
$sb = New-Object System.Text.StringBuilder
for ($p = 0; $p -lt 8; $p++) {
    [void]$sb.AppendLine("    // page $p (LCD rows $($p*8)..$($p*8+7))")
    for ($i = 0; $i -lt 8; $i++) {
        $vals = for ($j = 0; $j -lt 16; $j++) { '0x{0:X2}' -f $out[($p*128) + ($i*16) + $j] }
        $comma = if ($p -eq 7 -and $i -eq 7) { '' } else { ',' }
        [void]$sb.AppendLine('    ' + ($vals -join ', ') + $comma)
    }
}
$incPath = Join-Path $PSScriptRoot 'apex_emblem.inc'
[System.IO.File]::WriteAllText($incPath, $sb.ToString(), [System.Text.Encoding]::ASCII)
Write-Host ("Wrote {0} ({1} bytes)" -f (Split-Path $incPath -Leaf), $out.Length)

# --- ASCII preview (ink = '#') ----------------------------------------------
$n = 0
for ($y = 0; $y -lt $LCD_H; $y++) { for ($x = 0; $x -lt $LCD_W; $x++) { if ($ink[$x, $y]) { $n++ } } }
Write-Host ("`nDither=$Dither  ink $n / 8192 ({0:N1}%)" -f (100.0*$n/8192.0))
Write-Host ('+' + ('-' * $LCD_W) + '+')
for ($y = 0; $y -lt $LCD_H; $y++) {
    $s = New-Object System.Text.StringBuilder ($LCD_W + 2)
    [void]$s.Append('|')
    for ($x = 0; $x -lt $LCD_W; $x++) { if ($ink[$x, $y]) { [void]$s.Append('#') } else { [void]$s.Append('.') } }
    [void]$s.Append('|')
    Write-Host $s.ToString()
}
Write-Host ('+' + ('-' * $LCD_W) + '+')

