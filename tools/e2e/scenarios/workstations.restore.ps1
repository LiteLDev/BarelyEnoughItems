# Restores the BEI config backed up by workstations.prelaunch.ps1.
. "$PSScriptRoot/../lib.ps1"
$cfgDir = Join-Path $script:ModsDir 'BEI\config'
foreach ($f in 'config.json', 'bookmarks.json') {
    $p = Join-Path $cfgDir $f
    if (Test-Path "$p.bei-e2e-bak") { Move-Item "$p.bei-e2e-bak" $p -Force; Write-Step "restored $p" }
    elseif (Test-Path $p) { Remove-Item $p; Write-Step "removed seeded $p" }
}
