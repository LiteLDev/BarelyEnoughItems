# Restores the game options and BEI config backed up by showcase.prelaunch.ps1.
. "$PSScriptRoot/../lib.ps1"
$files = @((Get-ChildItem (Get-GameDataDir) -Recurse -Filter 'options.txt.bei-e2e-bak' -ErrorAction SilentlyContinue).FullName)
$cfgDir = Join-Path $script:ModsDir 'BEI\config'
foreach ($f in 'config.json', 'bookmarks.json') {
    $p = Join-Path $cfgDir $f
    if (Test-Path "$p.bei-e2e-bak") { $files += "$p.bei-e2e-bak" }
    elseif (Test-Path $p) { Remove-Item $p; Write-Step "removed seeded $p" }
}
foreach ($f in $files) {
    if ($f -and (Test-Path $f)) { Move-Item $f ($f -replace '\.bei-e2e-bak$', '') -Force; Write-Step "restored $($f -replace '\.bei-e2e-bak$', '')" }
}
