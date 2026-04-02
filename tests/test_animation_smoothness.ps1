# Animation Smoothness Integration Tests
# These tests verify the code changes for smooth player model animations
# are correctly integrated and don't regress aimbot accuracy paths.
# Run: pwsh tests/test_animation_smoothness.ps1

$ErrorActionPreference = "Stop"
$script:testsPassed = 0
$script:testsFailed = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if ($Condition) {
        Write-Host "  PASS: $Message" -ForegroundColor Green
        $script:testsPassed++
    } else {
        Write-Host "  FAIL: $Message" -ForegroundColor Red
        $script:testsFailed++
    }
}

function Assert-False {
    param([bool]$Condition, [string]$Message)
    Assert-True -Condition (-not $Condition) -Message $Message
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$hooksDir = Join-Path $repoRoot "SEOwnedDE\SEOwnedDE\src\App\Hooks"
$featuresDir = Join-Path $repoRoot "SEOwnedDE\SEOwnedDE\src\App\Features"

# ==============================================================================
Write-Host "`n=== Test Suite: CBaseEntity_AddVar.cpp ===" -ForegroundColor Cyan
# ==============================================================================
$addVarFile = Get-Content (Join-Path $hooksDir "CBaseEntity_AddVar.cpp") -Raw

Write-Host "  [Interpolation Var Registration]"

# TEST: Pose parameters should NOT be blocked (enables smooth animation blending)
Assert-False `
    -Condition ($addVarFile -match 'hash\s*==\s*m_iv_flPoseParameter') `
    -Message "m_iv_flPoseParameter is NOT blocked from interpolation (smooth pose blending)"

# TEST: Animation cycle should NOT be blocked (enables smooth playback position)
Assert-False `
    -Condition ($addVarFile -match 'hash\s*==\s*m_iv_flCycle') `
    -Message "m_iv_flCycle is NOT blocked from interpolation (smooth animation cycle)"

# TEST: Max ground speed should NOT be blocked (enables smooth speed transitions)
Assert-False `
    -Condition ($addVarFile -match 'hash\s*==\s*m_iv_flMaxGroundSpeed') `
    -Message "m_iv_flMaxGroundSpeed is NOT blocked from interpolation (smooth speed blending)"

Write-Host "  [Accuracy-Critical Vars Still Blocked]"

# TEST: Velocity interpolation MUST still be blocked (aimbot projectile prediction needs raw data)
Assert-True `
    -Condition ($addVarFile -match 'hash\s*==\s*m_iv_vecVelocity') `
    -Message "m_iv_vecVelocity IS still blocked (preserves aimbot velocity accuracy)"

# TEST: Eye angle interpolation for non-local MUST still be blocked (backstab detection)
Assert-True `
    -Condition ($addVarFile -match 'hash\s*==\s*m_iv_angEyeAngles') `
    -Message "m_iv_angEyeAngles IS still blocked for non-local players (backstab accuracy)"

# TEST: The hash constant declarations should still be present (not accidentally deleted)
Assert-True `
    -Condition ($addVarFile -match 'HASH_CT\("C_BaseEntity::m_iv_vecVelocity"\)') `
    -Message "Velocity hash constant is still declared"

# TEST: EstimateAbsVelocity hook is still present (returns raw netvar velocity)
Assert-True `
    -Condition ($addVarFile -match 'CBaseEntity_EstimateAbsVelocity') `
    -Message "EstimateAbsVelocity hook is still intact (raw velocity for aimbot)"

Assert-True `
    -Condition ($addVarFile -match 'pPlayer->m_vecVelocity\(\)') `
    -Message "EstimateAbsVelocity returns raw netvar velocity, not interpolated"

# ==============================================================================
Write-Host "`n=== Test Suite: CTFPlayer_UpdateClientSideAnimation.cpp ===" -ForegroundColor Cyan
# ==============================================================================
$updateAnimFile = Get-Content (Join-Path $hooksDir "CTFPlayer_UpdateClientSideAnimation.cpp") -Raw

Write-Host "  [Engine Animation Updates]"

# TEST: The bUpdatingAnims gate MUST block engine calls to preserve aimbot cycle accuracy and prevent fast-forwarding
Assert-True `
    -Condition ($updateAnimFile -match '!G::bUpdatingAnims.*?return') `
    -Message "bUpdatingAnims gate is PRESENT (preserves aimbot accuracy & prevents fast-forwarding)"

# TEST: CALL_ORIGINAL(ecx) should still be present (so engine calls still go through)
Assert-True `
    -Condition ($updateAnimFile -match 'CALL_ORIGINAL\(ecx\)') `
    -Message "CALL_ORIGINAL is present (animations actually run)"

Write-Host "  [Local Player Handling Preserved]"

# TEST: Local player special handling should still be intact
Assert-True `
    -Condition ($updateAnimFile -match 'ecx\s*==\s*pLocal') `
    -Message "Local player special handling is preserved"

# TEST: Viewmodel addon updates still happen for local player
Assert-True `
    -Condition ($updateAnimFile -match 'UpdateAllViewmodelAddons') `
    -Message "UpdateAllViewmodelAddons still called for local player"

# TEST: Halloween kart check still present
Assert-True `
    -Condition ($updateAnimFile -match 'TF_COND_HALLOWEEN_KART') `
    -Message "Halloween kart condition check still present"

# ==============================================================================
Write-Host "`n=== Test Suite: CSequenceTransitioner_CheckForSequenceChange.cpp ===" -ForegroundColor Cyan
# ==============================================================================
$seqTransFile = Get-Content (Join-Path $hooksDir "CSequenceTransitioner_CheckForSequenceChange.cpp") -Raw

Write-Host "  [Sequence Transition Blending]"

# TEST: bInterpolate should NOT be force-disabled
Assert-False `
    -Condition ($seqTransFile -match 'bInterpolate\s*=\s*false') `
    -Message "bInterpolate is NOT forced to false (sequence transitions blend smoothly)"

# TEST: CALL_ORIGINAL should pass bInterpolate through unchanged
Assert-True `
    -Condition ($seqTransFile -match 'CALL_ORIGINAL\(ecx,\s*hdr,\s*nCurSequence,\s*bForceNewSequence,\s*bInterpolate\)') `
    -Message "CALL_ORIGINAL passes bInterpolate through (engine controls blending)"

# TEST: The hook signature is still correct
Assert-True `
    -Condition ($seqTransFile -match 'CSequenceTransitioner_CheckForSequenceChange') `
    -Message "Hook signature for CSequenceTransitioner_CheckForSequenceChange is intact"

# ==============================================================================
Write-Host "`n=== Test Suite: Unchanged Systems Integration ===" -ForegroundColor Cyan
# ==============================================================================

Write-Host "  [SetupBones Optimization Unaffected]"

$setupBonesFile = Get-Content (Join-Path $hooksDir "CBaseAnimating_SetupBones.cpp") -Raw

# TEST: SetupBones optimization flag check evaluates accuracy improvements
Assert-True `
    -Condition ($setupBonesFile -match 'CFG::Misc_SetupBones_Optimization.*?&&.*?!CFG::Misc_Accuracy_Improvements') `
    -Message "SetupBones optimization selectively bypasses visual cache for smooth poses"

Assert-True `
    -Condition ($setupBonesFile -match 'GetCachedBoneData') `
    -Message "Bone cache retrieval logic is intact"

Assert-True `
    -Condition ($setupBonesFile -match 'vDelta\.LengthSqr\(\)') `
    -Message "Positional delta offset for cached bones is intact"

Write-Host "  [LagRecords System Unaffected]"

$lagRecordsFile = Get-Content (Join-Path $featuresDir "LagRecords\LagRecords.cpp") -Raw

Assert-True `
    -Condition ($lagRecordsFile -match 'AddRecord') `
    -Message "LagRecords::AddRecord is present"

Assert-True `
    -Condition ($lagRecordsFile -match 'SetupBones\(newRecord\.BoneMatrix') `
    -Message "Lag records still store server-tick bone matrices"

Assert-True `
    -Condition ($lagRecordsFile -match 'UpdateRecords') `
    -Message "LagRecords::UpdateRecords cleanup is present"

Write-Host "  [FrameStageNotify Animation Pipeline]"

$frameStageFile = Get-Content (Join-Path $hooksDir "IBaseClientDLL_FrameStageNotify.cpp") -Raw

Assert-True `
    -Condition ($frameStageFile -match 'bUpdatingAnims\s*=\s*true') `
    -Message "FrameStageNotify still sets bUpdatingAnims for manual anim pass"

Assert-True `
    -Condition ($frameStageFile -match 'UpdateClientSideAnimation') `
    -Message "FrameStageNotify still calls UpdateClientSideAnimation for accuracy"

Assert-True `
    -Condition ($frameStageFile -match 'LagRecords->AddRecord') `
    -Message "FrameStageNotify still adds lag records after anim updates"

Write-Host "  [BaseInterpolatePart1 Unaffected]"

$baseInterpFile = Get-Content (Join-Path $hooksDir "CBaseEntity_BaseInterpolatePart1.cpp") -Raw

Assert-True `
    -Condition ($baseInterpFile -match 'Shifting::bRecharging') `
    -Message "DT-shift recharging interp disable is preserved"

Assert-True `
    -Condition ($baseInterpFile -match 'CBaseDoor') `
    -Message "Door entity interp disable is preserved"

# ==============================================================================
Write-Host "`n=== Test Suite: DLL Build Artifact ===" -ForegroundColor Cyan
# ==============================================================================

$dllPath = Join-Path $repoRoot "SEOwnedDE\bin\ReleaseAVX2\SEOwnedDE.dll"
$buildRan = Test-Path $dllPath

if ($buildRan) {
    Assert-True -Condition $true -Message "SEOwnedDE.dll exists at expected output path"

    $dllSize = (Get-Item $dllPath).Length
    Assert-True `
        -Condition ($dllSize -gt 100KB) `
        -Message "DLL size is reasonable ($([math]::Round($dllSize/1KB))KB)"
} else {
    Write-Host "  SKIP: DLL artifact not present (build step may not have run yet)" -ForegroundColor Yellow
}

# ==============================================================================
# SUMMARY
# ==============================================================================
Write-Host "`n============================================" -ForegroundColor White
Write-Host "  Results: $($script:testsPassed) passed, $($script:testsFailed) failed" -ForegroundColor $(if ($script:testsFailed -eq 0) { "Green" } else { "Red" })
Write-Host "============================================`n" -ForegroundColor White

if ($script:testsFailed -gt 0) {
    exit 1
}

exit 0
