# Runs before the game starts (see run.ps1): seed bookmarks and a clean config so the overlay
# scenario sees a known state. The originals are restored at the end of overlay.ps1.
$script:OverlayCfgDir = Join-Path $script:ModsDir 'BEI\config'
New-Item -ItemType Directory -Force $script:OverlayCfgDir | Out-Null
$script:OverlayBackup = @{}
foreach ($f in 'config.json', 'bookmarks.json') {
    $p = Join-Path $script:OverlayCfgDir $f
    $script:OverlayBackup[$f] = if (Test-Path $p) { [IO.File]::ReadAllText($p) } else { $null }
}
$bookmarks = @{ version = 1; items = @(
        'minecraft:diamond', 'minecraft:oak_planks', 'minecraft:crafting_table', 'minecraft:iron_ingot',
        'minecraft:stick', 'minecraft:furnace', 'minecraft:torch'
    ) }
$config = @{
    version = 1; overlayEnabled = $true; bookmarksEnabled = $true; cheatMode = $false
    showOnAllContainers = $true; centerSearchBar = $false; maxColumns = 0; lastSearch = ''
}
[IO.File]::WriteAllText((Join-Path $script:OverlayCfgDir 'bookmarks.json'), ($bookmarks | ConvertTo-Json))
[IO.File]::WriteAllText((Join-Path $script:OverlayCfgDir 'config.json'), ($config | ConvertTo-Json))
Write-Step "seeded BEI config + bookmarks in $script:OverlayCfgDir"
