<#
.SYNOPSIS
  Run a BEI end-to-end scenario against the real game.

.DESCRIPTION
  Takes the machine-wide e2e lock, deploys bin\BEI, restores the BEI-Test world, launches
  Minecraft through LeviLauncher, enters the world, runs tools\e2e\scenarios\<Scenario>.ps1,
  checks the LeviLamina log for BEI errors and always closes the game and releases the lock.

  Screenshots land in tools\e2e\out\<Scenario>\. Review them (they are the evidence).

.EXAMPLE
  pwsh tools/e2e/run.ps1 -Scenario smoke
  pwsh tools/e2e/run.ps1 -Scenario smoke -KeepOpen   # leave the game running for manual poking
#>
param(
    [Parameter(Mandatory)] [string]$Scenario,
    [switch]$NoDeploy,
    [switch]$KeepOpen
)

. "$PSScriptRoot/lib.ps1"
$ErrorActionPreference = 'Stop'

$scenarioFile = Join-Path $PSScriptRoot "scenarios/$Scenario.ps1"
if (-not (Test-Path $scenarioFile)) { throw "Unknown scenario: $scenarioFile" }

Enter-E2ELock
$failed = $null
try {
    if (Get-GameProcess) { Write-Step "closing a running game first"; Stop-Game }
    if (-not $NoDeploy) { Deploy-Mod }
    Restore-TestWorld
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
    if (-not $KeepOpen) { Stop-Game }
    Exit-E2ELock
}
if ($failed) { exit 1 }
