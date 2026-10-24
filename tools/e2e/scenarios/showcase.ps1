# Showcase: stages and captures the screenshots and videos for docs/showcase/ (README and the
# LeviMC Mods gallery). Config, bookmarks and English language are seeded by showcase.prelaunch.ps1.
# Output: out/showcase/<name>.png (full-resolution stills) and out/showcase/rec-<name>/ (video
# frames + timestamps). Encode them with tools/e2e/showcase-encode.ps1.
$u = 'showcase'
. "$PSScriptRoot/../recorder.ps1"

# Client size of the captures (GUI scale follows the window size).
$script:ShotW = 1600; $script:ShotH = 900

function Get-Layout([string]$name) {
    $line = Wait-LogLine "$name layout:" 20
    $l = @{}
    foreach ($m in [regex]::Matches($line, '(\w+)=([-\d.]+)(?:,([-\d.]+))?')) {
        $l[$m.Groups[1].Value] = if ($m.Groups[3].Success) { @([double]$m.Groups[2].Value, [double]$m.Groups[3].Value) } else { [double]$m.Groups[2].Value }
    }
    return $l
}
function Cell([hashtable]$l, [int]$i) {
    $c = $i % $l.cols; $r = [Math]::Floor($i / $l.cols)
    return @(($l.cell0[0] + $c * $l.cell[0]), ($l.cell0[1] + $r * $l.cell[1]))
}
function Get-Pos([string]$what) {
    $line = Wait-LogLine "$what at [-\d.]+,[-\d.]+" 10
    $m = [regex]::Match($line, "$what at ([-\d.]+),([-\d.]+)")
    return @([double]$m.Groups[1].Value, [double]$m.Groups[2].Value)
}
# Wait for a log line that appears after $before matching lines were already there.
function Count-Log([string]$pattern) { @(Get-LeviLog | Select-String -Pattern $pattern).Count }
function Wait-NewLog([string]$pattern, [int]$before, [int]$TimeoutSeconds = 5) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hits = @(Get-LeviLog | Select-String -Pattern $pattern)
        if ($hits.Count -gt $before) { return $hits[-1].Line }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for a new log line '$pattern'"
}
# Move the cursor smoothly (so it is easy to follow in the videos).
$script:Cursor = @(0.5, 0.5)
function Glide([double]$x, [double]$y, [int]$ms = 450) {
    $steps = [Math]::Max(1, [int]($ms / 60))
    $x0 = $script:Cursor[0]; $y0 = $script:Cursor[1]
    for ($i = 1; $i -le $steps; $i++) {
        $t = $i / $steps; $e = $t * $t * (3 - 2 * $t)
        $rx = $x0 + ($x - $x0) * $e; $ry = $y0 + ($y - $y0) * $e
        if ($script:Headless) {
            Send-HeadlessMouse -Action move -RX $rx -RY $ry
            $c = Get-ClientRect
            [BeiE2E.Recorder]::VirtX = [int]($rx * $c.W); [BeiE2E.Recorder]::VirtY = [int]($ry * $c.H)
        } else {
            $p = ConvertTo-Screen $rx $ry -Relative
            [BeiE2E.Native]::SetCursorPos($p.X, $p.Y) | Out-Null
            [BeiE2E.Native]::MoveRelative(1, 0); [BeiE2E.Native]::MoveRelative(-1, 0)
        }
        Start-Sleep -Milliseconds 20
    }
    $script:Cursor = @($x, $y)
    Start-Sleep -Milliseconds 80
}
function Park([double]$x = 0.5, [double]$y = 0.03) { Move-Mouse $x $y -Relative; $script:Cursor = @($x, $y); Start-Sleep -Milliseconds 300 }
function LeftClick { [BeiE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 70; [BeiE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 200 }
function Type-Slowly([string]$text, [int]$ms = 130) {
    foreach ($c in $text.ToCharArray()) { [BeiE2E.Native]::Unicode($c); Start-Sleep -Milliseconds $ms }
}
function Ctrl-Click([double]$x, [double]$y) {
    [BeiE2E.Native]::Key(0x11, $false); Start-Sleep -Milliseconds 60
    Click $x $y -Relative
    [BeiE2E.Native]::Key(0x11, $true); Start-Sleep -Milliseconds 300
}
# Replace the search text (right click clears and focuses the box), then unfocus it.
function Set-Search([string]$text) {
    $p = Get-Layout 'Item panel'
    Click $p.extra[0] $p.extra[1] -Relative -Right; Start-Sleep -Milliseconds 200
    if ($text) { Type-Text $text }
    Press-Key ESC; Start-Sleep -Milliseconds 400
}
# Search for `text`, hover the first result and press R/U.
function Show-First([string]$text, [string]$key) {
    Set-Search $text
    $p = Get-Layout 'Item panel'
    Move-Mouse $p.cell0[0] $p.cell0[1] -Relative; Start-Sleep -Milliseconds 300
    $before = Count-Log 'Recipe screen opened'
    Press-Key $key
    Wait-NewLog 'Recipe screen opened' $before | Out-Null
    Start-Sleep -Milliseconds 1000
    Park 0.985 0.97
}

Wait-LogLine 'Recipe index rebuilt' 90 | Out-Null
Invoke-Chat '/gamerule sendcommandfeedback false'
Invoke-Chat '/gamerule commandblockoutput false'
Invoke-Chat '/gamerule domobspawning false'
Invoke-Chat '/kill @e[type=!player]'           # no animals wandering into the shots
Invoke-Chat '/time set 1000'
Invoke-Chat '/weather clear'
function Give-Kit {
    Invoke-Chat '/clear @s'
    foreach ($g in 'oak_log 24', 'oak_planks 40', 'cobblestone 48', 'iron_ingot 12', 'redstone 20', 'coal 16', 'bread 8', 'apple 5', 'diamond 3', 'torch 32') {
        Invoke-Chat "/give @s $g"
    }
}
Give-Kit
Start-Sleep -Seconds 12   # let the "New recipe unlocked" toasts fade
Set-GameWindowSize $script:ShotW $script:ShotH
Start-Sleep -Seconds 1
Focus-Game

# --- inventory with the item list and bookmarks -------------------------------------------------
Open-Inventory
Park
Save-Still 'inventory'

# --- bookmarks: hover the enchanted golden apple ----------------------------------------------
$b = Get-Layout 'Bookmark panel'
$p = Cell $b 4
Move-Mouse $p[0] $p[1] -Relative; Start-Sleep -Milliseconds 500
Save-Still 'bookmarks'

# --- search (video) ----------------------------------------------------------------------------
$items = Get-Layout 'Item panel'
Park 0.62 0.62
Start-Recording 'search'
Start-Sleep -Milliseconds 600
Glide $items.extra[0] $items.extra[1] 600
LeftClick
Start-Sleep -Milliseconds 300
Type-Slowly 'diamond'
Start-Sleep -Milliseconds 1400
Type-Slowly ' -ore'
Start-Sleep -Milliseconds 1600
[BeiE2E.Native]::MouseButton(1, $false); Start-Sleep -Milliseconds 70; [BeiE2E.Native]::MouseButton(1, $true)   # right click: clear
Start-Sleep -Milliseconds 500
Type-Slowly '#logs'
Start-Sleep -Milliseconds 1800
Stop-Recording
Press-Key ESC; Start-Sleep -Milliseconds 300

# --- recipes and uses --------------------------------------------------------------------------
Show-First '$minecraft:iron_ingot' R
Save-Still 'recipes'
Close-Screen
Show-First '$minecraft:iron_ingot' U
Save-Still 'uses'
Close-Screen
Show-First 'brewing stand' U
Save-Still 'brewing'
Close-Screen

# --- settings popup ----------------------------------------------------------------------------
Set-Search ''
$btn = Get-Pos 'Settings button'
Click $btn[0] $btn[1] -Relative
Wait-LogLine 'Settings popup opened' 5 | Out-Null
Park 0.5 0.03
Save-Still 'settings'
Click $btn[0] $btn[1] -Relative   # close
Start-Sleep -Milliseconds 300

# --- cheat mode (video): Ctrl+click the settings button, click items to take them ---------------
Set-Search 'diamond'
$items = Get-Layout 'Item panel'
Park 0.62 0.62
Start-Recording 'cheat'
Start-Sleep -Milliseconds 500
Glide $btn[0] $btn[1] 600
Start-Sleep -Milliseconds 500
Ctrl-Click $btn[0] $btn[1]
Start-Sleep -Milliseconds 900
foreach ($i in 0, 3, 7) {
    $p = Cell $items $i
    Glide $p[0] $p[1] 450
    Start-Sleep -Milliseconds 400
    LeftClick
    Start-Sleep -Milliseconds 500
}
Glide 0.62 0.62 500
Start-Sleep -Milliseconds 800
Stop-Recording
Ctrl-Click $btn[0] $btn[1]   # cheat mode off again
Set-Search ''
Close-Screen
Give-Kit
Start-Sleep -Seconds 10

# --- one-click transfer (video): crafting table in front of the player --------------------------
Invoke-Chat '/execute rotated ~ 0 run setblock ^ ^ ^2 crafting_table'
Invoke-Chat '/tp @s ~ ~ ~ ~ 30'
Start-Sleep -Seconds 1
Focus-Game
Click 0.5 0.5 -Relative -Right
Start-Sleep -Milliseconds 1500
Set-Search '$minecraft:piston -$_'
$items = Get-Layout 'Item panel'
Park 0.62 0.12
Start-Recording 'transfer'
Start-Sleep -Milliseconds 400
Glide $items.cell0[0] $items.cell0[1] 700
Start-Sleep -Milliseconds 600
$before = Count-Log 'Recipe transfer button: plus0='
LeftClick                                # recipes for the piston
$plus = [regex]::Match((Wait-NewLog 'Recipe transfer button: plus0=' $before), 'plus0=([-\d.]+),([-\d.]+)')
Start-Sleep -Milliseconds 600
Glide ([double]$plus.Groups[1].Value) ([double]$plus.Groups[2].Value) 700
Start-Sleep -Milliseconds 900
LeftClick                                # "+": ingredients into the grid, back to the table
Start-Sleep -Milliseconds 1200
Glide 0.62 0.62 600
Start-Sleep -Milliseconds 1200
Stop-Recording
Save-Still 'transfer-done'
Close-Screen
