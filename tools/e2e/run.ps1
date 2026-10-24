<#
.SYNOPSIS
  Run a BEI end-to-end scenario against the real game.

.DESCRIPTION
  Takes the machine-wide e2e lock, deploys bin\BEI, restores the BEI-Test world, launches
  Minecraft through LeviLauncher, enters the world, runs tools\e2e\scenarios\<Scenario>.ps1,
  checks the LeviLamina log for BEI errors and always closes the game and releases the lock.

  Screenshots land in tools\e2e\out\<Scenario>\. Review them (they are the evidence).

  By default the run is headless: the LeviHeadlessInput mod (https://github.com/LiteLDev/LeviHeadlessInput,
  see $script:HeadlessInputDir in lib.ps1) is deployed next to BEI and every click and key goes
  through its control pipe, so the game stays in the background and never takes the real mouse,
  keyboard or foreground. -Foreground uses real input (SendInput) instead.

.EXAMPLE
  pwsh tools/e2e/run.ps1 -Scenario smoke
  pwsh tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game running for manual poking
  pwsh tools/e2e/run.ps1 -Scenario smoke -Foreground # real mouse/keyboard (takes over the PC)
#>
param(
    [Parameter(Mandatory)] [string]$Scenario,
    [switch]$NoDeploy,
    [switch]$KeepOpen,
    [switch]$Foreground
)

. "$PSScriptRoot/lib.ps1"
$ErrorActionPreference = 'Stop'

if (-not $Foreground) {
    $shim = Join-Path $script:HeadlessInputDir 'harness-shim.ps1'
    if (-not (Test-Path $shim)) {
        throw "LeviHeadlessInput is not built ($shim missing). Build it, set `$env:LEVI_HEADLESS_INPUT_DIR, or pass -Foreground to use the real mouse and keyboard."
    }
    # Replaces Click/Press-Key/Type-Text/Scroll/Move-Mouse/Focus-Game/Assert-GameForeground/
    # Set-GameWindowSize and routes [BeiE2E.Native]::Key/MouseButton/... through the mod's pipe.
    . $shim
    Register-HeadlessNative ([BeiE2E.Native])
}

$scenarioFile = Join-Path $PSScriptRoot "scenarios/$Scenario.ps1"
if (-not (Test-Path $scenarioFile)) { throw "Unknown scenario: $scenarioFile" }

Enter-E2ELock
$failed = $null
try {
    if (Get-GameProcess) { Write-Step "closing a running game first"; Stop-Game }
    Disable-OtherMods
    if (-not $NoDeploy) { Deploy-Mod }
    $hiDir = Join-Path $script:ModsDir 'LeviHeadlessInput'
    if (Test-Path $hiDir) { Remove-Item -Recurse -Force $hiDir }   # leftover of a killed run
    if ($script:Headless) { Install-HeadlessInputMod -ModsDir $script:ModsDir -PipeName "LeviHeadlessInput-$script:VersionName" }
    Restore-TestWorld
    if ($script:Headless) { Set-RecentWorldsSinglePlayer }
    # Optional scenarios\<name>.prelaunch.ps1, e.g. to seed mods\BEI\config before the mod loads.
    $prelaunch = Join-Path $PSScriptRoot "scenarios/$Scenario.prelaunch.ps1"
    if (Test-Path $prelaunch) { . $prelaunch }
    Start-Game | Out-Null
    Wait-GameReady
    Enter-TestWorld
    $script:Unit = $Scenario
    . $scenarioFile
    Assert-NoErrors
    Write-Step "scenario '$Scenario' PASSED"
} catch {
    $failed = $_
    Write-Host "[e2e] scenario '$Scenario' FAILED: $_" -ForegroundColor Red
    try { Save-Screenshot 'failure' $Scenario | Out-Null } catch {}
    Get-LeviLog | Select-Object -Last 40 | ForEach-Object { Write-Host "  | $_" }
} finally {
    if (-not $KeepOpen) {
        try { Stop-Game } catch { Write-Host "[e2e] stopping the game failed: $_" -ForegroundColor Red }
        if ($script:Headless) { Uninstall-HeadlessInputMod -ModsDir $script:ModsDir }
        Restore-OtherMods
        # Optional scenarios\<name>.restore.ps1 undoes what the prelaunch script changed.
        $restore = Join-Path $PSScriptRoot "scenarios/$Scenario.restore.ps1"
        if (Test-Path $restore) { & pwsh -NoProfile -File $restore }
    }
    Exit-E2ELock
}
if ($failed) { exit 1 }
