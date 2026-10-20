# Workstation paths not covered elsewhere, through the real UI (config seeded with cheat mode by
# workstations.prelaunch.ps1, with
# the search bar centered under the container):
# - search box: the typed query is saved to config.json once the box is unfocused (debounced);
# - "+" transfer of a 3x3 recipe (chest) at a placed crafting table;
# - recipe-screen layouts with real data: brewing, smithing, stonecutter, cartography (U on the
#   station), and the special map recipes (R on a filled map);
# - creative mode: clicking the item panel while holding a stack deletes the stack.
# Screenshots: out/workstations/*.png
$u = 'workstations'
$script:PlusX = 0.6125; $script:PlusY = 0.374   # "+" of the first crafting recipe
Wait-LogLine 'Recipe index rebuilt' 90 | Out-Null
$cfgFile = Join-Path $script:ModsDir 'BEI\config\config.json'

$script:Failures = @()
function Fail([string]$msg) { Write-Step "FAIL: $msg"; $script:Failures += $msg }
function Count-Log([string]$pattern) { @(Get-LeviLog | Select-String -Pattern $pattern).Count }

# Latest "Item panel layout:" line (fractions of the client area), see PagedItemGrid::logLayoutIfChanged.
function Get-ItemPanel {
    $line = Wait-LogLine 'Item panel layout:' 20
    $l = @{}
    foreach ($m in [regex]::Matches($line, '(\w+)=([-\d.]+)(?:,([-\d.]+))?')) {
        $l[$m.Groups[1].Value] = if ($m.Groups[3].Success) { @([double]$m.Groups[2].Value, [double]$m.Groups[3].Value) } else { [double]$m.Groups[2].Value }
    }
    return $l
}
# Replace the panel search text (right click clears and focuses the box), then unfocus it.
function Set-Search([string]$text) {
    $p = Get-ItemPanel
    Click $p.extra[0] $p.extra[1] -Relative -Right; Start-Sleep -Milliseconds 300
    Type-Text $text
    Press-Key ESC   # unfocus (swallowed by BEI while the box is focused)
    Start-Sleep -Milliseconds 400
}
# Search for `text`, hover the first result and press a hotkey (R/U); waits for the recipe screen.
# Sets $script:Opened (records a failure when it did not open).
function Show-First([string]$text, [string]$key, [string]$shot) {
    Set-Search $text
    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 400
    $before = Count-Log 'Recipe screen opened'
    Press-Key $key
    Start-Sleep -Milliseconds 1200
    if ((Count-Log 'Recipe screen opened') -le $before) {
        Save-Screenshot $shot $u | Out-Null
        Fail "$key on '$text' did not open the recipe screen"
        $script:Opened = $false
        return
    }
    Write-Step (Wait-LogLine 'Recipe screen opened' 1)
    Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot $shot $u | Out-Null
    $script:Opened = $true
}

try {
    Invoke-Chat '/clear @s'
    Invoke-Chat '/give @s oak_planks 16'

    # --- crafting table: search, debounced save, "+" transfer of the chest recipe ------------
    Invoke-Chat '/setblock ~ ~-1 ~ crafting_table'
    Invoke-Chat '/tp @s ~ ~ ~ 0 90'
    Start-Sleep -Milliseconds 800
    Focus-Game
    Click 0.5 0.5 -Relative -Right
    Start-Sleep -Milliseconds 1500
    Save-Screenshot 'table' $u | Out-Null

    $p = Get-ItemPanel
    Click $p.extra[0] $p.extra[1] -Relative -Right; Start-Sleep -Milliseconds 300
    Type-Text '$minecraft:chest -$_'   # the plain chest only
    $typing = (Get-Content -Raw $cfgFile | ConvertFrom-Json).lastSearch
    Write-Step "lastSearch while typing: '$typing'"
    Start-Sleep -Milliseconds 1500   # idle: the debounced save runs
    $idle = (Get-Content -Raw $cfgFile | ConvertFrom-Json).lastSearch
    if ($idle -ne '$minecraft:chest -$_') { Fail "lastSearch after 1.5 s idle is '$idle'" }
    Press-Key ESC
    Start-Sleep -Milliseconds 400
    Save-Screenshot 'search_chest' $u | Out-Null

    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 400
    Press-Key R
    Start-Sleep -Milliseconds 1200
    Write-Step (Wait-LogLine 'Recipe screen opened: minecraft:chest' 5)
    Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot 'chest_recipes' $u | Out-Null
    # "+" next to the first recipe (crafting category, 1600x900 at GUI scale 3).
    Move-Mouse $script:PlusX $script:PlusY -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'chest_plus_hover' $u | Out-Null
    Click $script:PlusX $script:PlusY -Relative
    Start-Sleep -Milliseconds 1200
    try { Write-Step (Wait-LogLine 'Actions: transferred recipe .* into 3x3 grid: ok' 5) } catch { Fail '"+" did not transfer the chest recipe into the 3x3 grid' }
    Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot 'chest_transferred' $u | Out-Null

    # --- recipe-screen layouts with real data (U on the station shows its whole category) -----
    Show-First 'brewing stand' U 'brewing'
    if ($script:Opened) {
        Press-Key RIGHT; Start-Sleep -Milliseconds 400
        Save-Screenshot 'brewing_page2' $u | Out-Null
        Close-Screen
    }
    Show-First 'smithing table' U 'smithing'
    if ($script:Opened) {
        Press-Key RIGHT; Start-Sleep -Milliseconds 400
        Save-Screenshot 'smithing_page2' $u | Out-Null
        Close-Screen
    }
    Show-First 'stonecutter' U 'stonecutter'
    if ($script:Opened) {
        Close-Screen
    }
    Show-First 'cartography table' U 'cartography'
    if ($script:Opened) {
        Press-Key RIGHT; Start-Sleep -Milliseconds 400
        Save-Screenshot 'cartography_page2' $u | Out-Null
        Close-Screen
    }
    Show-First '$minecraft:filled_map' R 'filled_map_recipes'
    if ($script:Opened) {
        Press-Key RIGHT; Start-Sleep -Milliseconds 400
        Save-Screenshot 'filled_map_recipes_page2' $u | Out-Null
        Press-Key DOWN; Start-Sleep -Milliseconds 400
        Save-Screenshot 'filled_map_recipes_tab2' $u | Out-Null
        Close-Screen
    }
    # Empty map + compass is a data-driven recipe (the hardcoded MapUpgradingRecipe needs a filled map).
    Show-First 'empty locator map' R 'empty_locator_map_recipes'
    if ($script:Opened) {
        Close-Screen
    }
    Set-Search ''
    Close-Screen

    # --- ground truth for the map upgrading recipe: what does the 2x2 grid accept? -----------
    # Survival inventory, 1600x900 GUI scale 3: hotbar slot i at (0.506 + 0.0338 i, 0.732),
    # crafting grid cells at (0.672 + 0.0338 c, 0.31 + 0.06 r), output at (0.778, 0.34).
    function Try-Craft([string]$first, [string]$shot) {
        Invoke-Chat '/clear @s'
        Invoke-Chat "/give @s $first 1"
        Invoke-Chat '/give @s compass 1'
        Open-Inventory
        Click 0.506 0.732 -Relative; Start-Sleep -Milliseconds 300
        Click 0.672 0.31 -Relative; Start-Sleep -Milliseconds 300
        Click 0.540 0.732 -Relative; Start-Sleep -Milliseconds 300
        Click 0.706 0.31 -Relative; Start-Sleep -Milliseconds 500
        Move-Mouse 0.778 0.34 -Relative; Start-Sleep -Milliseconds 500
        Save-Screenshot $shot $u | Out-Null
        Close-Screen   # the grid items go back to the inventory
    }
    Try-Craft 'empty_map' 'craft_empty_map_compass'
    # A real filled map: use an empty map (hotbar slot 1 selected).
    Invoke-Chat '/clear @s'
    Invoke-Chat '/give @s empty_map 1'
    Invoke-Chat '/tp @s ~ ~ ~ 0 0'   # look away from the crafting table
    Press-Key 1
    Click 0.5 0.5 -Relative -Right; Start-Sleep -Milliseconds 1500
    Invoke-Chat '/give @s compass 1'
    Open-Inventory
    Click 0.506 0.732 -Relative; Start-Sleep -Milliseconds 300
    Click 0.672 0.31 -Relative; Start-Sleep -Milliseconds 300
    Click 0.540 0.732 -Relative; Start-Sleep -Milliseconds 300
    Click 0.706 0.31 -Relative; Start-Sleep -Milliseconds 500
    Move-Mouse 0.778 0.34 -Relative; Start-Sleep -Milliseconds 500
    Save-Screenshot 'craft_filled_map_compass' $u | Out-Null
    Close-Screen

    # --- creative: click the item panel while holding a stack deletes it -------------------
    Invoke-Chat '/clear @s'
    Invoke-Chat '/give @s oak_planks 16'
    Invoke-Chat '/gamemode creative'
    Open-Inventory
    Start-Sleep -Milliseconds 500
    Save-Screenshot 'creative_inventory' $u | Out-Null
    Click 0.365 0.803 -Relative; Start-Sleep -Milliseconds 400   # creative hotbar slot 1: stack onto the cursor
    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot 'creative_holding' $u | Out-Null
    $before = Count-Log 'BEI] Actions: cursor item deleted'
    Click $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 800
    if ((Count-Log 'BEI] Actions: cursor item deleted') -le $before) { Fail 'clicking the panel did not delete the cursor stack' }
    Move-Mouse 0.5 0.95 -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot 'creative_deleted' $u | Out-Null
    Close-Screen
    Invoke-Chat '/gamemode survival'
} finally {
    if ($script:Failures.Count) { throw "workstations: $($script:Failures.Count) failure(s): $($script:Failures -join '; ')" }
}
