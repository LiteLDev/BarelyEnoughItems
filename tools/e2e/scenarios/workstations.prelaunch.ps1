# Runs before the game starts (see run.ps1): back up the BEI config and seed one with cheat mode and the centered search bar
# on and an empty search, so workstations.ps1 sees a known state. Restored by workstations.restore.ps1.
$cfgDir = Join-Path $script:ModsDir 'BEI\config'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
foreach ($f in 'config.json', 'bookmarks.json') {
    $p = Join-Path $cfgDir $f
    if ((Test-Path $p) -and -not (Test-Path "$p.bei-e2e-bak")) { Copy-Item $p "$p.bei-e2e-bak" }
}
$config = @{
    version = 1; overlayEnabled = $true; bookmarksEnabled = $true; cheatMode = $true
    showOnAllContainers = $true; centerSearchBar = $true; maxColumns = 0; lastSearch = ''
}
[IO.File]::WriteAllText((Join-Path $cfgDir 'config.json'), ($config | ConvertTo-Json))
[IO.File]::WriteAllText((Join-Path $cfgDir 'bookmarks.json'), (@{ version = 1; items = @() } | ConvertTo-Json))
Write-Step "seeded BEI config (cheat mode) in $cfgDir"
