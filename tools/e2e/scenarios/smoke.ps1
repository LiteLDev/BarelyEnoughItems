# Smoke test: the mod loads, indexes items/recipes, draws the item panel on the inventory
# screen, and mouse coordinates map correctly (hover tooltip on the first panel item).
# Screenshots: out/smoke/{hud,inventory,hover}.png
$u = 'smoke'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null
Save-Screenshot 'hud' $u | Out-Null
Open-Inventory
Save-Screenshot 'inventory' $u | Out-Null
Move-Mouse 0.836 0.083 -Relative   # first cell of the item panel at 1600x900
Start-Sleep -Milliseconds 500
Save-Screenshot 'hover' $u | Out-Null
Close-Screen
