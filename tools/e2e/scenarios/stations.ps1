# Recipe transfer beyond crafting, and the anvil / composting categories, through the real UI
# (config seeded with cheat mode and the centered search bar by stations.prelaunch.ps1):
# - furnace (placed with /setblock): "+" on an iron ingot smelting recipe moves one input and a
#   stack of coal into the empty fuel slot; shift-"+" moves a whole stack of the input;
#   with no inputs left the "+" stays, tinted red;
# - smithing table: "+" on the netherite sword recipe fills template, base and addition;
# - anvil category: R on a diamond sword, U on an iron ingot; composting: U on the composter.
# Screenshots: out/stations/*.png
$u = 'stations'
Wait-LogLine 'Recipe index rebuilt' 90 | Out-Null

$script:Failures = @()
function Fail([string]$msg) { Write-Step "FAIL: $msg"; $script:Failures += $msg }
function Count-Log([string]$pattern) { @(Get-LeviLog | Select-String -Pattern $pattern).Count }
# Waits for a matching line that appears after `$before` matches existed; returns it or $null.
function Wait-NewLog([string]$pattern, [int]$before, [int]$timeoutSeconds = 6) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hits = @(Get-LeviLog | Select-String -Pattern $pattern)
        if ($hits.Count -gt $before) { return $hits[-1].Line }
        Start-Sleep -Milliseconds 300
    }
    return $null
}

# Latest "Item panel layout:" line (fractions of the client area), see PagedItemGrid::logLayoutIfChanged.
function Get-ItemPanel {
    $line = Wait-LogLine 'Item panel layout:' 20
    $l = @{}
    foreach ($m in [regex]::Matches($line, '(\w+)=([-\d.]+)(?:,([-\d.]+))?')) {
        $l[$m.Groups[1].Value] = if ($m.Groups[3].Success) { @([double]$m.Groups[2].Value, [double]$m.Groups[3].Value) } else { [double]$m.Groups[2].Value }
    }
    return $l
}
function Set-Search([string]$text) {
    $p = Get-ItemPanel
    Click $p.extra[0] $p.extra[1] -Relative -Right; Start-Sleep -Milliseconds 300
    Type-Text $text
    Press-Key ESC
    Start-Sleep -Milliseconds 400
}
# Search, hover the first result, press R/U. True when the recipe screen opened.
function Show-First([string]$text, [string]$key) {
    Set-Search $text
    $p = Get-ItemPanel
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 400
    $before = Count-Log 'Recipe screen opened'
    Press-Key $key
    if (-not (Wait-NewLog 'Recipe screen opened' $before 5)) { Fail "$key on '$text' did not open the recipe screen"; return $false }
    Start-Sleep -Milliseconds 800
    return $true
}
# Press DOWN until the recipe screen shows `category` (tabs wrap around). True if found.
function Select-Tab([string]$category) {
    $tabPattern = "Recipe screen tab: $([regex]::Escape($category))$"
    # RecipeLayer logs the shown tab whenever it changes, so the latest line is the current tab.
    $current = Get-LeviLog | Select-String -Pattern 'Recipe screen tab: ' | Select-Object -Last 1
    if ($current -and $current.Line -match $tabPattern) {
        Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
        return $true
    }
    for ($i = 0; $i -lt 24; $i++) {
        $before = Count-Log 'Recipe screen tab: '
        Press-Key DOWN
        $line = Wait-NewLog 'Recipe screen tab: ' $before 3
        if ($line -and $line -match $tabPattern) {
            Start-Sleep -Milliseconds 800
            Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
            return $true
        }
    }
    Fail "tab $category not found"
    return $false
}
# The first "+" of the shown page (RecipeLayer logs it on change): @{ x; y; ready } or $null.
function Get-Plus {
    $hit = Get-LeviLog | Select-String -Pattern 'Recipe transfer button: ' | Select-Object -Last 1
    if (-not $hit -or $hit.Line -notmatch 'plus0=([\d.]+),([\d.]+) ready=(\w+)') { return $null }
    return @{ x = [double]$Matches[1]; y = [double]$Matches[2]; ready = $Matches[3] -eq 'true' }
}
function Click-Plus($plus, [switch]$Shift) {
    Move-Mouse $plus.x $plus.y -Relative; Start-Sleep -Milliseconds 300
    if ($Shift) { [BeiE2E.Native]::Key([uint16]0x10, $false); Start-Sleep -Milliseconds 100 }
    try { Click $plus.x $plus.y -Relative } finally { if ($Shift) { [BeiE2E.Native]::Key([uint16]0x10, $true) } }
    Start-Sleep -Milliseconds 800
}
function Open-Below([string]$block, [string]$readyLog) {
    Invoke-Chat "/setblock ~ ~-1 ~ $block"
    Invoke-Chat '/tp @s ~ ~ ~ 0 90'
    Start-Sleep -Milliseconds 800
    Focus-Game
    $before = Count-Log $readyLog
    Click 0.5 0.5 -Relative -Right
    if (-not (Wait-NewLog $readyLog $before 6)) { Fail "$block screen not detected"; return $false }
    Start-Sleep -Milliseconds 600
    return $true
}
# Opens the iron ingot recipes on the furnace tab and returns its first "+" (or $null).
function Show-FurnacePlus([string]$shot) {
    if (-not (Show-First '$minecraft:iron_ingot' R)) { return $null }
    if (-not (Select-Tab 'minecraft:furnace')) { Close-Screen; return $null }
    Save-Screenshot $shot $u | Out-Null
    $plus = Get-Plus
    if (-not $plus) { Fail "no '+' on the furnace tab ($shot)"; Close-Screen }
    return $plus
}

try {
    # --- furnace ---------------------------------------------------------------------------
    Invoke-Chat '/clear @s'
    if (Open-Below 'furnace' 'Actions: furnace screen active \(tag .furnace.') {
        # No inputs: the "+" is shown, tinted red (missing), and does nothing.
        $plus = Show-FurnacePlus 'furnace_missing'
        if ($plus) {
            if ($plus.ready) { Fail 'furnace "+" ready with an empty inventory' }
            Move-Mouse $plus.x $plus.y -Relative; Start-Sleep -Milliseconds 400
            Save-Screenshot 'furnace_missing_hover' $u | Out-Null
            $before = Count-Log 'Actions: transferred recipe'
            Click-Plus $plus
            if (Wait-NewLog 'Actions: transferred recipe' $before 1) { Fail 'a red "+" transferred something' }
            Close-Screen
        }
        Set-Search ''
        Close-Screen
    }
    # Whichever iron ingot recipe is listed first, the player has its input.
    Invoke-Chat '/give @s raw_iron 64'
    Invoke-Chat '/give @s iron_ore 64'
    Invoke-Chat '/give @s deepslate_iron_ore 64'
    Invoke-Chat '/give @s coal 10'
    if (Open-Below 'furnace' 'Actions: furnace screen active \(tag .furnace.') {
        Save-Screenshot 'furnace_open' $u | Out-Null
        $plus = Show-FurnacePlus 'furnace_tab'
        if ($plus) {
            if (-not $plus.ready) { Fail 'furnace "+" not ready although the player has the inputs' }
            Move-Mouse $plus.x $plus.y -Relative; Start-Sleep -Milliseconds 400
            Save-Screenshot 'furnace_plus_hover' $u | Out-Null
            $before = Count-Log 'Actions: transferred recipe .* into minecraft:furnace: ok'
            $fuelBefore = Count-Log 'Actions: added fuel minecraft:coal x10'
            Click-Plus $plus
            if (-not (Wait-NewLog 'Actions: transferred recipe .* \(1 set\) into minecraft:furnace: ok' $before)) { Fail '"+" did not move one input into the furnace' }
            if (-not (Wait-NewLog 'Actions: added fuel minecraft:coal x10' $fuelBefore 2)) { Fail '"+" did not add the coal stack as fuel' }
            Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
            Save-Screenshot 'furnace_transferred' $u | Out-Null
        }

        # Shift: a whole stack of the input (the input slot is cleared back to the inventory first).
        $plus = Show-FurnacePlus 'furnace_tab_again'
        if ($plus) {
            $before = Count-Log 'Actions: transferred recipe .* into minecraft:furnace: ok'
            Click-Plus $plus -Shift
            $line = Wait-NewLog 'Actions: transferred recipe .* into minecraft:furnace: ok' $before
            if (-not $line) { Fail 'shift-"+" did not transfer' }
            elseif ($line -notmatch '\(64 sets\)') { Fail "shift-'+' did not move a full stack: $line" }
            Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
            Save-Screenshot 'furnace_transferred_stack' $u | Out-Null
        }

        # Wrong station: smoker recipes have no "+" in a furnace.
        if (Show-First 'smoker' U) {
            if (Select-Tab 'minecraft:smoker') {
                Start-Sleep -Milliseconds 500
                $line = (Get-LeviLog | Select-String -Pattern 'Recipe transfer button: ' | Select-Object -Last 1).Line
                if ($line -notmatch 'Recipe transfer button: none') { Fail "smoker recipes show a '+' in a furnace: $line" }
                Save-Screenshot 'smoker_in_furnace' $u | Out-Null
            }
            Close-Screen
        }
        Set-Search ''
        Close-Screen
    }

    # --- smithing table ----------------------------------------------------------------------
    Invoke-Chat '/clear @s'
    Invoke-Chat '/give @s netherite_upgrade_smithing_template 2'
    Invoke-Chat '/give @s diamond_sword 1'
    Invoke-Chat '/give @s netherite_ingot 2'
    if (Open-Below 'smithing_table' 'Actions: smithing table screen active') {
        Save-Screenshot 'smithing_open' $u | Out-Null
        if (Show-First '$minecraft:netherite_sword' R) {
            if (Select-Tab 'minecraft:smithing') {
                Save-Screenshot 'smithing_tab' $u | Out-Null
                $plus = Get-Plus
                if (-not $plus) { Fail "no '+' on the smithing tab"; Close-Screen }
                else {
                    if (-not $plus.ready) { Fail 'smithing "+" not ready although the player has the inputs' }
                    $before = Count-Log 'Actions: transferred recipe .* into minecraft:smithing: ok'
                    Click-Plus $plus -Shift   # shift: still one set (the sword does not stack)
                    if (-not (Wait-NewLog 'Actions: transferred recipe .* \(1 set\) into minecraft:smithing: ok' $before)) { Fail '"+" did not fill the smithing table' }
                    Move-Mouse 0.97 0.97 -Relative; Start-Sleep -Milliseconds 300
                    Save-Screenshot 'smithing_transferred' $u | Out-Null
                }
            } else { Close-Screen }
        }
        Set-Search ''
        Close-Screen
    }

    # --- anvil and composting categories ------------------------------------------------------
    Invoke-Chat '/clear @s'
    Open-Inventory
    if (Show-First '$minecraft:diamond_sword' R) {
        if (Select-Tab 'minecraft:anvil') { Save-Screenshot 'anvil_diamond_sword' $u | Out-Null }
        Close-Screen
    }
    if (Show-First '$minecraft:iron_ingot' U) {
        if (Select-Tab 'minecraft:anvil') {
            Save-Screenshot 'anvil_iron_ingot' $u | Out-Null
            Press-Key RIGHT; Start-Sleep -Milliseconds 400
            Save-Screenshot 'anvil_iron_ingot_page2' $u | Out-Null
        }
        Close-Screen
    }
    if (Show-First '$minecraft:composter' U) {
        if (Select-Tab 'minecraft:composting') { Save-Screenshot 'composting' $u | Out-Null }
        Close-Screen
    }
    Set-Search ''
    Close-Screen

    $log = Get-LeviLog
    foreach ($cat in 'minecraft:anvil', 'minecraft:composting') {
        $line = $log | Where-Object { $_ -match "Recipes \[$([regex]::Escape($cat))\]: (\d+)" } | Select-Object -Last 1
        if (-not $line) { Fail "no count logged for $cat" } else { Write-Step $line }
    }
} finally {
    if ($script:Failures.Count) { throw "stations: $($script:Failures.Count) failure(s): $($script:Failures -join '; ')" }
}
