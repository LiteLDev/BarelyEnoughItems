# Widgets: search box editing (typing through the text-char hook, caret keys, selection,
# clipboard, horizontal scrolling, keyboard ownership) and the item tooltip.
# Screenshots: out/widgets/*.png
$u = 'widgets'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null

function Key-Down([int]$vk) { [BeiE2E.Native]::Key([uint16]$vk, $false); Start-Sleep -Milliseconds 40 }
function Key-Up([int]$vk) { [BeiE2E.Native]::Key([uint16]$vk, $true); Start-Sleep -Milliseconds 40 }
function Tap([int]$vk) { Key-Down $vk; Key-Up $vk; Start-Sleep -Milliseconds 150 }
function Ctrl-Tap([char]$key) { Key-Down 0x11; Tap ([int]$key); Key-Up 0x11; Start-Sleep -Milliseconds 150 }
$VK_HOME = 0x24; $VK_END = 0x23; $VK_DELETE = 0x2E

Open-Inventory
Save-Screenshot '01_open' $u | Out-Null

Click 0.9 0.957 -Relative            # focus the panel search box
Start-Sleep -Milliseconds 300
Save-Screenshot '02_focused' $u | Out-Null

Type-Text 'oak'
Start-Sleep -Milliseconds 400
Save-Screenshot '03_typed_oak' $u | Out-Null
try { Wait-LogLine 'Text input routed to BEI' 5 | Out-Null } catch { Write-Step "WARN: no routed text input in the log: $_" }

Press-Key LEFT                        # o a | k
Press-Key BACK                        # o | k   -> "ok"
Start-Sleep -Milliseconds 300
Save-Screenshot '04_edit_ok' $u | Out-Null
Type-Text 'a'                         # "oak" again, caret after 'a'
Tap $VK_END
Type-Text ' log'                      # "oak log"
Start-Sleep -Milliseconds 300
Save-Screenshot '05_oak_log' $u | Out-Null

Tap $VK_HOME
Tap $VK_DELETE                        # "ak log"
Start-Sleep -Milliseconds 300
Save-Screenshot '06_home_delete' $u | Out-Null

Ctrl-Tap 'A'                          # select all (visible selection)
Start-Sleep -Milliseconds 300
Save-Screenshot '07_select_all' $u | Out-Null
Type-Text 'stone'                     # replaces the selection
Start-Sleep -Milliseconds 300
Save-Screenshot '08_replaced_stone' $u | Out-Null

Ctrl-Tap 'A'; Ctrl-Tap 'X'            # cut -> empty
Start-Sleep -Milliseconds 300
Save-Screenshot '09_cut' $u | Out-Null
Ctrl-Tap 'V'; Ctrl-Tap 'V'            # paste twice -> "stonestone"
Start-Sleep -Milliseconds 300
Save-Screenshot '10_paste_twice' $u | Out-Null

Ctrl-Tap 'A'
Type-Text '木'                        # Unicode / CJK through the text path
Start-Sleep -Milliseconds 300
Save-Screenshot '11_cjk' $u | Out-Null

Ctrl-Tap 'A'
Type-Text 'abcdefghijklmnopqrstuvwxyz0123456789'
Start-Sleep -Milliseconds 300
Save-Screenshot '12_scroll_end' $u | Out-Null
Tap $VK_HOME
Start-Sleep -Milliseconds 300
Save-Screenshot '13_scroll_home' $u | Out-Null

Ctrl-Tap 'A'
Type-Text 'oak'
Press-Key E                           # focused: types 'e', must not close the inventory
Press-Key W                           # types 'w', must not move the player
Save-Screenshot '14_e_while_focused' $u | Out-Null   # right after an edit: caret visible

Press-Key ESC                         # unfocus only; inventory stays open
Start-Sleep -Milliseconds 400
Save-Screenshot '15_unfocused' $u | Out-Null

Click 0.9 0.957 -Relative -Right      # right click clears (and focuses)
Start-Sleep -Milliseconds 300
Save-Screenshot '16_right_click_clear' $u | Out-Null
Press-Key ENTER                       # unfocus
Start-Sleep -Milliseconds 300

Move-Mouse 0.836 0.083 -Relative      # first cell: tooltip
Start-Sleep -Milliseconds 500
Save-Screenshot '17_tooltip' $u | Out-Null

Move-Mouse 0.95 0.083 -Relative       # tooltip near the right edge stays on screen
Start-Sleep -Milliseconds 500
Save-Screenshot '18_tooltip_edge' $u | Out-Null

Press-Key E                           # unfocused: E closes the inventory
Start-Sleep -Milliseconds 800
Save-Screenshot '19_closed' $u | Out-Null
