# Input routing: hotkeys over vanilla slots and BEI panels (R/U/A/O), wheel routing, no hotkeys
# in gameplay or while typing, and BEI's keys in the controls menu.
# Relies on BEI's "Hotkey ..." info log lines. Screenshots: out/input/*.png
$u = 'input'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null

$script:Failures = @()
function Fail([string]$msg) { Write-Step "FAIL: $msg"; $script:Failures += $msg }
function Count-Log([string]$pattern) { @(Get-LeviLog | Select-String -Pattern $pattern).Count }
function Assert-Hotkey([string]$key, [string]$pattern, [string]$what) {
    $before = Count-Log $pattern
    Press-Key $key
    Start-Sleep -Milliseconds 500
    if ((Count-Log $pattern) -le $before) { Fail "hotkey $key $what did not fire" }
}

$bookmarks = Join-Path $script:ModsDir 'BEI\config\bookmarks.json'
function Read-Bookmarks { if (Test-Path $bookmarks) { Get-Content -Raw $bookmarks } else { '' } }

Invoke-Chat '/clear @s'
Invoke-Chat '/give @s oak_log 8'
Open-Inventory
Save-Screenshot 'inventory' $u | Out-Null

# Vanilla slot: first hotbar slot of the survival inventory (1600x900, GUI scale 3).
Move-Mouse 0.506 0.732 -Relative
Start-Sleep -Milliseconds 400
Save-Screenshot 'hover-vanilla' $u | Out-Null
Assert-Hotkey R 'Hotkey show_recipes on minecraft:oak_log' 'on a vanilla slot'
Assert-Hotkey U 'Hotkey show_uses on minecraft:oak_log' 'on a vanilla slot'
Write-Step 'R/U on vanilla slot ok'

# Holding a key (OS auto-repeat) acts once.
$before = Count-Log 'Hotkey show_recipes on'
Press-Key R 900
Start-Sleep -Milliseconds 300
$n = (Count-Log 'Hotkey show_recipes on') - $before
if ($n -ne 1) { Fail "holding R fired $n times" }

# Empty background: nothing hovered, R does nothing.
Move-Mouse 0.5 0.03 -Relative
Start-Sleep -Milliseconds 400
$before = Count-Log 'Hotkey show_recipes on'
Press-Key R
Start-Sleep -Milliseconds 400
if ((Count-Log 'Hotkey show_recipes on') -gt $before) { Fail 'R fired with nothing hovered' }

# BEI item panel: A toggles the bookmark of the first panel item (and back).
Move-Mouse 0.836 0.083 -Relative
Start-Sleep -Milliseconds 400
Save-Screenshot 'hover-panel' $u | Out-Null
$bm0 = Read-Bookmarks
Assert-Hotkey A 'Hotkey toggle_bookmark on' 'on the item panel'
Start-Sleep -Milliseconds 300
$bm1 = Read-Bookmarks
Save-Screenshot 'bookmark-toggled' $u | Out-Null
if ($bm0 -eq $bm1) { Fail 'A over the panel did not change bookmarks.json' }
Write-Step "bookmarks.json changed by A"
Move-Mouse 0.836 0.083 -Relative
Assert-Hotkey A 'Hotkey toggle_bookmark on' 'on the item panel (restore)'
Start-Sleep -Milliseconds 300
function Sorted([string]$json) { (($json | ConvertFrom-Json).items | Sort-Object) -join ',' }
if ((Sorted (Read-Bookmarks)) -ne (Sorted $bm0)) { Fail 'second A did not restore the bookmark set' }
Assert-Hotkey R 'Hotkey show_recipes on' 'on the item panel'

# Wheel over the panel (swallowed, routed to BEI) and over the vanilla inventory.
Move-Mouse 0.9 0.4 -Relative
Scroll -1
Start-Sleep -Milliseconds 400
Save-Screenshot 'scroll-panel' $u | Out-Null
Scroll 1
Move-Mouse 0.506 0.732 -Relative
Scroll -1
Scroll 1

# O hides the item list; O again shows it.
Assert-Hotkey O 'Hotkey toggle_overlay: item list hidden' 'to hide the item list'
Save-Screenshot 'overlay-off' $u | Out-Null
Assert-Hotkey O 'Hotkey toggle_overlay: item list shown' 'to show the item list'
Save-Screenshot 'overlay-on' $u | Out-Null
Close-Screen

# Gameplay: R/U/A/O must do nothing, and A must still strafe.
$before = Count-Log 'Hotkey '
Save-Screenshot 'gameplay-before' $u | Out-Null
Press-Key R; Press-Key U; Press-Key O
Press-Key A 800
Start-Sleep -Milliseconds 400
Save-Screenshot 'gameplay-after-strafe' $u | Out-Null
if ((Count-Log 'Hotkey ') -gt $before) { Fail 'hotkey fired during gameplay' }

# Chat text box: typing must not trigger hotkeys.
Press-Key T; Start-Sleep -Milliseconds 700
Press-Key R; Press-Key A; Press-Key U; Press-Key O
Start-Sleep -Milliseconds 300
Save-Screenshot 'chat-typed' $u | Out-Null
Press-Key ESC; Start-Sleep -Milliseconds 500
if ((Count-Log 'Hotkey ') -gt $before) { Fail 'hotkey fired while typing in chat' }

# Best effort: the controls menu lists BEI's keys with readable names.
Press-Key ESC; Start-Sleep -Milliseconds 800
Save-Screenshot 'after-chat' $u | Out-Null
Press-Key ESC; Start-Sleep -Milliseconds 1500
Save-Screenshot 'pause' $u | Out-Null
Click 0.293 0.46 -Relative; Start-Sleep -Milliseconds 2500
Save-Screenshot 'settings' $u | Out-Null
Click 0.11 0.556 -Relative; Start-Sleep -Milliseconds 1500   # Controls > Keyboard & Mouse
Move-Mouse 0.66 0.6 -Relative
for ($i = 0; $i -lt 60; $i++) { Scroll -3 }                     # mod keys are listed last
Start-Sleep -Milliseconds 800
Save-Screenshot 'controls-bottom' $u | Out-Null
for ($j = 0; $j -lt 20; $j++) { Scroll 1 }                    # BEI's keys sit just above "Chords"
Start-Sleep -Milliseconds 600
Save-Screenshot 'controls-bei' $u | Out-Null

if ($script:Failures.Count -gt 0) { throw "input scenario: $($script:Failures.Count) failure(s): $($script:Failures -join '; ')" }
Write-Step 'input scenario passed'
