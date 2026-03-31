# Aimbot Aim Assist Integration Tests
# These tests verify the code changes for the new Aim Assist method
# Run: pwsh tests/test_aimbot_aimassist.ps1

$ErrorActionPreference = "Stop"
$FailedTests = 0

Write-Host "Running Aim Assist Integration Tests..." -ForegroundColor Cyan

# Define paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$CFGPath = Join-Path $ProjectRoot "SEOwnedDE\SEOwnedDE\src\App\Features\CFG.h"
$AimbotHitscanPath = Join-Path $ProjectRoot "SEOwnedDE\SEOwnedDE\src\App\Features\Aimbot\AimbotHitscan\AimbotHitscan.cpp"
$MenuPath = Join-Path $ProjectRoot "SEOwnedDE\SEOwnedDE\src\App\Features\Menu\Menu.cpp"

# Helper function
function Check-Content {
    param(
        [string]$FilePath,
        [string]$SearchPattern,
        [string]$TestDescription
    )

    $Content = Get-Content $FilePath -Raw
    if ($Content -match $SearchPattern) {
        Write-Host "[PASS] $TestDescription" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] $TestDescription" -ForegroundColor Red
        Write-Host "       Could not find pattern '$SearchPattern' in $FilePath" -ForegroundColor DarkGray
        $script:FailedTests++
    }
}

# 1. Test CFG.h Configuration
Write-Host "`n1. Checking Configuration (CFG.h)" -ForegroundColor Yellow
Check-Content -FilePath $CFGPath -SearchPattern "CFGVAR\(Aimbot_Hitscan_Aim_Type,\s*1\);\s*//.*3\s*Aim\s*Assist" -TestDescription "Aimbot_Hitscan_Aim_Type comment updated for Aim Assist (3)"
Check-Content -FilePath $CFGPath -SearchPattern "CFGVAR\(Aimbot_Hitscan_AimAssist_Strength,\s*15\.0f\);" -TestDescription "Aimbot_Hitscan_AimAssist_Strength var added with default 15.0f"

# 2. Test AimbotHitscan.cpp Logic
Write-Host "`n2. Checking Aimbot Logic (AimbotHitscan.cpp)" -ForegroundColor Yellow
Check-Content -FilePath $AimbotHitscanPath -SearchPattern "Aim_Type\s*==\s*2\s*\|\|\s*CFG::Aimbot_Hitscan_Aim_Type\s*==\s*3" -TestDescription "ShouldAim handles type 3 (Aim Assist) for Sniper logic"
Check-Content -FilePath $AimbotHitscanPath -SearchPattern "case\s*3:[\s\S]*?AimAssist_Strength" -TestDescription "Aim() function has switch case 3 using AimAssist_Strength divisor"
Check-Content -FilePath $AimbotHitscanPath -SearchPattern "Advanced_Smooth_AutoShoot\s*&&\s*\(?CFG::Aimbot_Hitscan_Aim_Type\s*==\s*2\s*\|\|\s*CFG::Aimbot_Hitscan_Aim_Type\s*==\s*3\)?" -TestDescription "ShouldFire handles type 3 (Aim Assist) for Advanced Smooth AutoShoot"

# 3. Test Menu UI integration
Write-Host "`n3. Checking Menu UI (Menu.cpp)" -ForegroundColor Yellow
Check-Content -FilePath $MenuPath -SearchPattern '\{\s*"Aim\s*Assist",\s*3\s*\}' -TestDescription "Aim Type dropdown contains Aim Assist option (3)"
Check-Content -FilePath $MenuPath -SearchPattern 'SliderFloat\("Aim\s*Assist\s*Strength",\s*CFG::Aimbot_Hitscan_AimAssist_Strength' -TestDescription "Slider added for Aim Assist Strength"

Write-Host "`n----------------------------------------"
if ($FailedTests -eq 0) {
    Write-Host "All Aim Assist integration tests PASSED!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$FailedTests tests FAILED!" -ForegroundColor Red
    exit 1
}
