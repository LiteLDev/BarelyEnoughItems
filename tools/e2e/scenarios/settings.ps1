# In-game settings, the focus-search hotkey, search history and the cursor stack in other
# containers, through the real UI (config seeded by settings.prelaunch.ps1):
# - settings button (wrench) right of the search bar: tooltip, popup, toggles persisted to
#   config.json, max columns -/+, Ctrl+click toggles cheat mode (red button);
# - Ctrl+F focuses the search box; Enter submits into searchHistory; Up/Down browse it;
# - chest: the held stack is drawn above the BEI panels (a panel click while holding it does
#   nothing, also in creative); creative inventory + cheat mode: the click deletes the stack.
# Relies on BEI's "Settings ..." / "Hotkey ..." info log lines. Screenshots: out/settings/*.png
$u = 'settings'
Wait-LogLine 'Recipe index rebuilt' 90 | Out-Null
$cfgFile = Join-Path $script:ModsDir 'BEI\config\config.json'

$script:Failures = @()
function Fail([string]$msg) { Write-Step "FAIL: $msg"; $script:Failures += $msg }
function Count-Log([string]$pattern) { @(Get-LeviLog | Select-String -Pattern $pattern).Count }
function Read-Config { Get-Content -Raw $cfgFile | ConvertFrom-Json }
function Key-Down([int]$vk) { [BeiE2E.Native]::Key([uint16]$vk, $false); Start-Sleep -Milliseconds 40 }
function Key-Up([int]$vk) { [BeiE2E.Native]::Key([uint16]$vk, $true); Start-Sleep -Milliseconds 40 }
$VK_CTRL = 0x11
function Ctrl-Click([double]$x, [double]$y) { Key-Down $VK_CTRL; Click $x $y -Relative; Key-Up $VK_CTRL; Start-Sleep -Milliseconds 300 }
function Ctrl-F { Key-Down $VK_CTRL; Press-Key F; Key-Up $VK_CTRL; Start-Sleep -Milliseconds 400 }
# "<what> at x,y" (fractions of the client area) from the latest matching log line.
function Get-Pos([string]$what) {
    $line = Wait-LogLine "$what at [-\d.]+,[-\d.]+" 10
    $m = [regex]::Match($line, "$what at ([-\d.]+),([-\d.]+)")
    return @([double]$m.Groups[1].Value, [double]$m.Groups[2].Value)
}
function Get-ItemPanel {
    $line = Wait-LogLine 'Item panel layout:' 20
    $l = @{}
    foreach ($m in [regex]::Matches($line, '(\w+)=([-\d.]+)(?:,([-\d.]+))?')) {
        $l[$m.Groups[1].Value] = if ($m.Groups[3].Success) { @([double]$m.Groups[2].Value, [double]$m.Groups[3].Value) } else { [double]$m.Groups[2].Value }
    }
    return $l
}
# Click a popup row and wait for its "Settings: <id> = <value>" line.
function Toggle-Row([string]$id) {
    $pos = Get-Pos "Settings row $id"
    $before = Count-Log "Settings: $id = "
    Click $pos[0] $pos[1] -Relative
    Start-Sleep -Milliseconds 400
    if ((Count-Log "Settings: $id = ") -le $before) { Fail "clicking the $id row did nothing" }
}

try {
    Invoke-Chat '/clear @s'
    Invoke-Chat '/give @s oak_planks 16'
    Open-Inventory

    # --- settings button and popup ----------------------------------------------------------
    $btn = Get-Pos 'Settings button'
    Move-Mouse $btn[0] $btn[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot '01_button_tooltip' $u | Out-Null
    Click $btn[0] $btn[1] -Relative
    try { Wait-LogLine 'Settings popup opened' 5 | Out-Null } catch { Fail 'the settings popup did not open' }
    Start-Sleep -Milliseconds 300
    Save-Screenshot '02_popup' $u | Out-Null

    Toggle-Row 'showItemIds'
    if (-not (Read-Config).showItemIds) { Fail 'showItemIds was not saved' }
    Toggle-Row 'bookmarksAddToFront'
    if ((Read-Config).bookmarksAddToFront) { Fail 'bookmarksAddToFront was not saved' }
    Toggle-Row 'bookmarksAddToFront'

    $minus = Get-Pos 'Settings row maxColumnsMinus'
    Click $minus[0] $minus[1] -Relative; Start-Sleep -Milliseconds 400
    Click $minus[0] $minus[1] -Relative; Start-Sleep -Milliseconds 600
    $cols = (Read-Config).maxColumns
    if ($cols -le 0) { Fail "maxColumns after two '-' is $cols" }
    Save-Screenshot '03_columns_minus' $u | Out-Null
    $plus = Get-Pos 'Settings row maxColumnsPlus'
    Click $plus[0] $plus[1] -Relative; Start-Sleep -Milliseconds 400
    Click $plus[0] $plus[1] -Relative; Start-Sleep -Milliseconds 600
    $cols = (Read-Config).maxColumns
    if ($cols -ne 0) { Fail "maxColumns after two '+' is $cols (expected 0 = auto)" }

    Toggle-Row 'centerSearchBar'
    Start-Sleep -Milliseconds 300
    Save-Screenshot '04_center_search' $u | Out-Null
    # The button moved with the bar (and the popup with it): close and reopen it there.
    $btn = Get-Pos 'Settings button'
    Click $btn[0] $btn[1] -Relative; Start-Sleep -Milliseconds 300   # close
    Click $btn[0] $btn[1] -Relative; Start-Sleep -Milliseconds 300   # open again
    Toggle-Row 'centerSearchBar'
    if ((Read-Config).centerSearchBar) { Fail 'centerSearchBar not switched back off' }
    $btn = Get-Pos 'Settings button'

    # A click elsewhere closes the popup.
    Click 0.3 0.1 -Relative; Start-Sleep -Milliseconds 400
    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 500
    Save-Screenshot '05_tooltip_with_id' $u | Out-Null

    # --- Ctrl+click: cheat mode -------------------------------------------------------------
    Ctrl-Click $btn[0] $btn[1]
    if (-not (Read-Config).cheatMode) { Fail 'Ctrl+click did not enable cheat mode' }
    Move-Mouse $btn[0] $btn[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot '06_cheat_on' $u | Out-Null
    Ctrl-Click $btn[0] $btn[1]
    if ((Read-Config).cheatMode) { Fail 'Ctrl+click did not disable cheat mode' }

    # --- Ctrl+F and search history ----------------------------------------------------------
    Move-Mouse 0.3 0.1 -Relative
    $before = Count-Log 'Hotkey focus_search: search box focused'
    Ctrl-F
    if ((Count-Log 'Hotkey focus_search: search box focused') -le $before) { Fail 'Ctrl+F did not focus the search box' }
    Type-Text 'oak'
    Press-Key ENTER; Start-Sleep -Milliseconds 500
    Ctrl-F
    Type-Text 'stone'          # Ctrl+F selected all: replaces "oak"
    Press-Key ENTER; Start-Sleep -Milliseconds 500
    $hist = @((Read-Config).searchHistory)
    if (($hist -join ',') -ne 'stone,oak') { Fail "searchHistory is '$($hist -join ',')' (expected stone,oak)" }
    Save-Screenshot '07_stone' $u | Out-Null
    Ctrl-F
    Press-Key UP; Start-Sleep -Milliseconds 1500   # -> "oak" (the current text "stone" is skipped)
    Save-Screenshot '08_history_up' $u | Out-Null
    $last = (Read-Config).lastSearch
    if ($last -ne 'oak') { Fail "Up: lastSearch is '$last' (expected oak)" }
    Press-Key DOWN; Start-Sleep -Milliseconds 1500  # back to what was in the box
    $last = (Read-Config).lastSearch
    if ($last -ne 'stone') { Fail "Down: lastSearch is '$last' (expected stone)" }
    Press-Key ESC; Start-Sleep -Milliseconds 300
    Click 0.9 0.957 -Relative -Right; Start-Sleep -Milliseconds 300   # clear
    Press-Key ESC; Start-Sleep -Milliseconds 300
    Close-Screen

    # --- chest: held stack above the panels -------------------------------------------------
    Invoke-Chat '/setblock ~ ~-1 ~ chest'
    Invoke-Chat '/tp @s ~ ~ ~ 0 90'
    Start-Sleep -Milliseconds 800
    Focus-Game
    Click 0.5 0.5 -Relative -Right
    Start-Sleep -Milliseconds 1500
    Save-Screenshot '09_chest' $u | Out-Null
    $rectLine = Wait-LogLine 'Container rect on \w*chest' 10
    Write-Step $rectLine
    $m = [regex]::Match($rectLine, ': ([-\d.]+),([-\d.]+) ([-\d.]+)x([-\d.]+)')
    $p = Get-ItemPanel
    $gw = 18 / $p.cell[0]; $gh = 18 / $p.cell[1]   # screen size in GUI units
    $cx = [double]$m.Groups[1].Value; $cy = [double]$m.Groups[2].Value
    $cw = [double]$m.Groups[3].Value; $ch = [double]$m.Groups[4].Value
    $slotX = ($cx + 16) / $gw; $slotY = ($cy + $ch - 16) / $gh   # first hotbar slot
    Write-Step "chest hotbar slot 0 at $slotX,$slotY"
    Click $slotX $slotY -Relative; Start-Sleep -Milliseconds 400
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot '10_chest_holding_over_panel' $u | Out-Null
    # Drop it outside the window (survival: vanilla drops the stack).
    Click 0.02 0.5 -Relative; Start-Sleep -Milliseconds 600
    Save-Screenshot '11_chest_dropped' $u | Out-Null
    Close-Screen

    # --- creative + cheat mode ---------------------------------------------------------------
    # In a chest, clicking the item panel while holding a stack does nothing (there is no creative
    # item list to drop it on; the stack stays on the cursor, drawn above the panel). In the
    # creative inventory the same click deletes the stack.
    Invoke-Chat '/give @s oak_planks 16'
    Invoke-Chat '/gamemode creative'
    Focus-Game
    Click 0.5 0.5 -Relative -Right
    Start-Sleep -Milliseconds 1500
    $btn = Get-Pos 'Settings button'
    Ctrl-Click $btn[0] $btn[1]
    if (-not (Read-Config).cheatMode) { Fail 'Ctrl+click in the chest did not enable cheat mode' }
    Click $slotX $slotY -Relative; Start-Sleep -Milliseconds 400
    $p = Get-ItemPanel
    $before = Count-Log 'BEI] Actions: cursor item deleted'
    Click $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 800
    if ((Count-Log 'BEI] Actions: cursor item deleted') -gt $before) { Fail 'a panel click in a chest claimed to delete the stack' }
    Save-Screenshot '12_creative_chest_still_holding' $u | Out-Null
    Click $slotX $slotY -Relative; Start-Sleep -Milliseconds 400   # put it back
    Close-Screen

    Open-Inventory
    Click 0.365 0.803 -Relative; Start-Sleep -Milliseconds 400   # creative hotbar slot 1
    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot '13_creative_inventory_holding' $u | Out-Null
    $before = Count-Log 'BEI] Actions: cursor item deleted'
    Click $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 800
    if ((Count-Log 'BEI] Actions: cursor item deleted') -le $before) { Fail 'clicking the panel in the creative inventory did not delete the cursor stack' }
    Move-Mouse 0.5 0.95 -Relative; Start-Sleep -Milliseconds 300
    Save-Screenshot '14_creative_inventory_deleted' $u | Out-Null
    Close-Screen
    Invoke-Chat '/gamemode survival'
} finally {
    if ($script:Failures.Count) { throw "settings: $($script:Failures.Count) failure(s): $($script:Failures -join '; ')" }
}
