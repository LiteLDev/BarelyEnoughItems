# Runs before the game starts (see run.ps1): English game language, hand and HUD hidden, and a tidy BEI config with a
# hand-picked bookmark list, for the showcase shots. Originals are backed up next to the files
# (*.bei-e2e-bak) and restored by showcase.restore.ps1.
$opts = Get-ChildItem (Get-GameDataDir) -Recurse -Filter options.txt -ErrorAction SilentlyContinue | Select-Object -First 1
if ($opts) {
    Copy-Item $opts.FullName "$($opts.FullName).bei-e2e-bak" -Force
    # English, and no hand or HUD in front of the screens.
    (Get-Content $opts.FullName) -replace '^game_language:.*$', 'game_language:en_US' -replace '^gfx_hidehand:.*$', 'gfx_hidehand:1' -replace '^gfx_hidehud:.*$', 'gfx_hidehud:1' |
        Set-Content $opts.FullName -Encoding UTF8
}
$cfgDir = Join-Path $script:ModsDir 'BEI\config'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
foreach ($f in 'config.json', 'bookmarks.json') {
    $p = Join-Path $cfgDir $f
    if ((Test-Path $p) -and -not (Test-Path "$p.bei-e2e-bak")) { Copy-Item $p "$p.bei-e2e-bak" }
}
$config = @{
    version = 1; overlayEnabled = $true; bookmarksEnabled = $true; cheatMode = $false
    showOnAllContainers = $true; centerSearchBar = $false; maxColumns = 0; lastSearch = ''
    showItemIds = $false; showModName = $true; rememberPage = $false; searchHistory = @()
    bookmarksAddToFront = $false
}
$bookmarks = @{ version = 1; items = @(
        'minecraft:diamond', 'minecraft:emerald', 'minecraft:netherite_ingot', 'minecraft:golden_apple',
        'minecraft:enchanted_golden_apple', 'minecraft:ender_pearl', 'minecraft:blaze_rod', 'minecraft:redstone',
        'minecraft:piston', 'minecraft:beacon', 'minecraft:elytra', 'minecraft:totem_of_undying'
    ) }
[IO.File]::WriteAllText((Join-Path $cfgDir 'config.json'), ($config | ConvertTo-Json))
[IO.File]::WriteAllText((Join-Path $cfgDir 'bookmarks.json'), ($bookmarks | ConvertTo-Json))
Write-Step "seeded BEI config + bookmarks and English language for the showcase"
