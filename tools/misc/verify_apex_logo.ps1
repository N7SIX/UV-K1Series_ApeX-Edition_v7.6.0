# End-to-end check of the ApeX SetSav logo pipeline:
#   1. the BITMAP_ApexLogo / BITMAP_ApexLogoInv initializers in App/bitmaps.c
#      each have exactly 1024 bytes,
#   2. they match the converter outputs (tools/misc/apex_logo.inc and
#      tools/misc/apex_logo_inv.inc), and the inverted one is the exact
#      bitwise inverse of the normal one,
#   3. those bytes are present unchanged in the linked firmware binary,
#   4. the "ApeX" and "ApeX-I" menu labels made it into the binary.
# Run from the repository root, after building the ApeX preset:
#   powershell -ExecutionPolicy Bypass -File tools\misc\verify_apex_logo.ps1 [build-dir]

$ErrorActionPreference = 'Stop'
$buildDir = if ($args.Count -gt 0) { $args[0] } else { 'build/ApeX-wincheck' }
$elf = Get-ChildItem $buildDir -Filter '*.elf' | Select-Object -First 1
$binFile = Get-ChildItem $buildDir -Filter '*.bin' | Select-Object -First 1
if (-not $elf -or -not $binFile) { throw "No .elf/.bin in $buildDir" }

$flashOrigin = 0x08002800   # FLASH ORIGIN in Core/py32f071xb.ld

function Get-Bytes([string]$text, [string]$symbol) {
    $m = [regex]::Match($text, [regex]::Escape($symbol) + '\[1024\]\s*=\s*\{(.*?)\};', 'Singleline')
    if (-not $m.Success) { throw "$symbol not found" }
    return @([regex]::Matches($m.Groups[1].Value, '0x([0-9A-Fa-f]{2})') |
        ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
}

function Get-BinAddr([string]$elfPath, [string]$symbol) {
    $line = (& arm-none-eabi-nm $elfPath | Select-String (" " + $symbol + "$")).Line
    if (-not $line) { throw "$symbol symbol missing from the ELF" }
    return [Convert]::ToUInt32(($line -split '\s+')[0], 16)
}

# --- 1. parse the arrays from App/bitmaps.c -------------------------------
$src = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot '..\..\App\bitmaps.c'))
$cvals = Get-Bytes $src 'BITMAP_ApexLogo'
$cvalsInv = Get-Bytes $src 'BITMAP_ApexLogoInv'
if ($cvals.Count -ne 1024) { throw "BITMAP_ApexLogo has $($cvals.Count) bytes, expected 1024" }
if ($cvalsInv.Count -ne 1024) { throw "BITMAP_ApexLogoInv has $($cvalsInv.Count) bytes, expected 1024" }
Write-Host '1. App/bitmaps.c BITMAP_ApexLogo + BITMAP_ApexLogoInv: 1024 bytes each OK'

# --- 2. compare against the converter outputs ------------------------------
$inc = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot 'apex_logo.inc'))
$incInv = [System.IO.File]::ReadAllText((Join-Path $PSScriptRoot 'apex_logo_inv.inc'))
$ivals = @([regex]::Matches($inc, '0x([0-9A-Fa-f]{2})') |
    ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
$ivalsInv = @([regex]::Matches($incInv, '0x([0-9A-Fa-f]{2})') |
    ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) })
$diff = 0
for ($i = 0; $i -lt 1024; $i++) { if ($cvals[$i] -ne $ivals[$i]) { $diff++ } }
if ($diff -ne 0) { throw "bitmaps.c vs converter output: $diff mismatching bytes (ApeX)" }
$diff = 0
for ($i = 0; $i -lt 1024; $i++) { if ($cvalsInv[$i] -ne $ivalsInv[$i]) { $diff++ } }
if ($diff -ne 0) { throw "bitmaps.c vs converter output: $diff mismatching bytes (ApeX-I)" }
$diff = 0
for ($i = 0; $i -lt 1024; $i++) { if ($cvalsInv[$i] -ne ($cvals[$i] -bxor 0xFF)) { $diff++ } }
if ($diff -ne 0) { throw "ApeX-I is not the exact inverse of ApeX: $diff mismatching bytes" }
Write-Host '2. App/bitmaps.c matches converter outputs, ApeX-I is the exact inverse OK'

# --- 3. locate the symbols and compare the linked binary --------------------
$bin = [System.IO.File]::ReadAllBytes($binFile.FullName)
foreach ($sym in @('BITMAP_ApexLogo', 'BITMAP_ApexLogoInv')) {
    $addr = Get-BinAddr $elf.FullName $sym
    $off = $addr - $flashOrigin
    $ref = if ($sym -eq 'BITMAP_ApexLogo') { $cvals } else { $cvalsInv }
    $diff = 0
    for ($i = 0; $i -lt 1024; $i++) { if ($bin[$off + $i] -ne $ref[$i]) { $diff++ } }
    if ($diff -ne 0) { throw "ELF .bin vs source ($sym): $diff mismatching bytes at 0x$($addr.ToString('X'))" }
    Write-Host ("3. Linked binary matches {0} at 0x{1} OK" -f $sym, $addr.ToString('X'))
}

# --- 4. menu labels ---------------------------------------------------------
$txt = [System.Text.Encoding]::ASCII.GetString($bin)
if (-not $txt.Contains('ApeX')) { throw "Menu label 'ApeX' missing from the binary" }
if (-not $txt.Contains('ApeX-I')) { throw "Menu label 'ApeX-I' missing from the binary" }
Write-Host "4. 'ApeX' + 'ApeX-I' menu labels present OK"

Write-Host 'ALL CHECKS PASSED'