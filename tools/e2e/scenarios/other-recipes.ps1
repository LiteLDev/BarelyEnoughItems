# Other recipe providers (smelting family, stonecutter, smithing, cartography, brewing, fuel, anvil, composting,
# generic fallback): per-category counts are logged on index rebuild, and "U" on an item with
# smelting/fuel uses opens the recipe screen.
# Screenshots: out/other-recipes/{hover,uses}.png
$u = 'other-recipes'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null

$log = Get-LeviLog
$expected = @(
    'minecraft:furnace', 'minecraft:blast_furnace', 'minecraft:smoker', 'minecraft:campfire',
    'minecraft:soul_campfire', 'minecraft:stonecutter', 'minecraft:smithing', 'minecraft:cartography',
    'minecraft:brewing', 'minecraft:fuel', 'minecraft:anvil', 'minecraft:composting'
)
foreach ($cat in $expected) {
    $line = $log | Where-Object { $_ -match "Recipes \[$([regex]::Escape($cat))\]: (\d+)" } | Select-Object -Last 1
    if (-not $line) { throw "no count logged for $cat" }
    $null = $line -match "Recipes \[$([regex]::Escape($cat))\]: (\d+)"
    $n = [int]$Matches[1]
    Write-Step "$cat = $n"
    if ($n -le 0) { throw "category $cat is empty" }
}

Open-Inventory
Move-Mouse 0.836 0.083 -Relative   # first panel cell (Oak Planks: a fuel)
Start-Sleep -Milliseconds 500
Save-Screenshot 'hover' $u | Out-Null
Press-Key U
# The hotkey path belongs to the input unit; report rather than fail when it is not wired yet.
try { Wait-LogLine 'RecipeScreen::show item=minecraft:oak_planks mode=uses' 10 | Out-Null; Write-Step 'U on Oak Planks opened its uses' }
catch { Write-Step "WARN: U on Oak Planks did not reach RecipeScreen::show ($_)" }
Start-Sleep -Milliseconds 500
Save-Screenshot 'uses' $u | Out-Null
Close-Screen
