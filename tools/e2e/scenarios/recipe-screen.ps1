# Recipe screen: open from the item panel, hover/tooltip, paging (button, wheel, keys), tabs,
# uses (right click), Backspace (back) and ESC (close).
# Screenshots: out/recipe-screen/*.png
# Positions are for 1600x900 at GUI scale 3 (533x300 GUI units) with the crafting category
# selected (panel 200 wide, centered).
$u = 'recipe-screen'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null
Open-Inventory
Move-Mouse 0.836 0.083 -Relative          # first item of the panel
Start-Sleep -Milliseconds 400
Save-Screenshot '00-inventory-hover' $u | Out-Null

Click 0.836 0.083 -Relative               # left click: recipes for it
Start-Sleep -Milliseconds 1200
Wait-LogLine 'Recipe screen opened' 10 | Out-Null
Move-Mouse 0.95 0.95 -Relative
Start-Sleep -Milliseconds 300
Save-Screenshot '01-recipes' $u | Out-Null

Move-Mouse 0.392 0.243 -Relative          # first grid slot of the first recipe
Start-Sleep -Milliseconds 500
Save-Screenshot '02-slot-tooltip' $u | Out-Null
Start-Sleep -Milliseconds 1100
Save-Screenshot '02b-cycled' $u | Out-Null

Click 0.665 0.173 -Relative               # next page button
Start-Sleep -Milliseconds 400
Save-Screenshot '03-next-page' $u | Out-Null

Move-Mouse 0.5 0.6 -Relative
Scroll -1                                 # wheel down: next page (wraps)
Start-Sleep -Milliseconds 400
Save-Screenshot '04-wheel' $u | Out-Null

Click 0.379 0.053 -Relative               # second tab
Start-Sleep -Milliseconds 400
Save-Screenshot '05-tab2' $u | Out-Null

Press-Key DOWN                            # next category
Start-Sleep -Milliseconds 400
Save-Screenshot '06-key-down' $u | Out-Null

Click 0.336 0.053 -Relative               # first tab again
Start-Sleep -Milliseconds 400
Click 0.392 0.243 -Right -Relative        # uses of the first ingredient
Start-Sleep -Milliseconds 600
Move-Mouse 0.95 0.95 -Relative
Start-Sleep -Milliseconds 300
Save-Screenshot '07-uses' $u | Out-Null
Click 0.379 0.053 -Relative               # second tab of the uses (another category)
Start-Sleep -Milliseconds 400
Move-Mouse 0.95 0.95 -Relative
Start-Sleep -Milliseconds 300
Save-Screenshot '07b-uses-tab2' $u | Out-Null

Press-Key BACK                            # back to the previous focus (tab/page restored)
Start-Sleep -Milliseconds 600
Save-Screenshot '08-back' $u | Out-Null

Press-Key RIGHT                           # next page
Start-Sleep -Milliseconds 400
Save-Screenshot '09-key-right' $u | Out-Null

Press-Key ESC                             # close: back to the inventory
Start-Sleep -Milliseconds 1000
Save-Screenshot '10-closed' $u | Out-Null
Wait-LogLine 'Recipe screen closed' 5 | Out-Null
Close-Screen
Start-Sleep -Milliseconds 500
Save-Screenshot '11-hud' $u | Out-Null
