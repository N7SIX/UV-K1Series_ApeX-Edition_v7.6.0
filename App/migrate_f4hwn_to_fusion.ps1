# PowerShell script to replace ENABLE_FEAT_F4HWN_ with ENABLE_FEAT_FUSION_ in action.c
$filePath = 'c:\Users\sebue\Documents\N7SIX\Software\Quansheng\UV-K1Series_ApeX-Edition_v7.6.0\App\app\action.c'

# Read the file
$content = Get-Content -Path $filePath -Raw

# Replace F4HWN with FUSION (this is a broad replacement that should be safe given the codebase structure)
$newContent = $content.Replace('ENABLE_FEAT_F4HWN_', 'ENABLE_FEAT_FUSION_')

# Write back
Set-Content -Path $filePath -Value $newContent

Write-Host "Migration complete: FEAT_F4HWN_ → FEAT_FUSION_ in action.c"