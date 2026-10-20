# Actions (unit 11): cheat-mode give, recipe transfer into the 2x2 / 3x3 grid, cursor delete.
#
# The Actions API has no UI of its own (the overlay and the recipe screen call it). With a
# BEI_DEV build (xmake f ... --cxflags=-DBEI_DEV) the mod registers a client command
# `/beidebug` that drives the API directly, and this scenario checks every action.
# With a normal build it only checks that the crafting-screen tracking hooks run without errors.
#
# Screenshots: out/actions/{inventory,give,transfer_planks,transfer_table,table_open,transfer_chest,delete}.png
$u = 'actions'
Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null

Invoke-Chat '/clear @s'
Invoke-Chat '/give @s oak_log 5'
Invoke-Chat '/give @s oak_planks 12'

$dev = [bool](Get-LeviLog | Select-String 'beidebug: dev command registered')
if (-not $dev) {
    Write-Step 'not a BEI_DEV build: only checking the screen hooks'
    Open-Inventory
    Save-Screenshot 'inventory' $u | Out-Null
    Close-Screen
    return
}

# --- cheat mode ------------------------------------------------------------------------------
Invoke-Chat '/beidebug cheat'
Wait-LogLine 'beidebug: canCheat=true' 20 | Out-Null
Invoke-Chat '/beidebug give minecraft:diamond 3'
Wait-LogLine 'beidebug: give minecraft:diamond 3 -> true' 20 | Out-Null
Invoke-Chat '/beidebug give minecraft:wool:14 2'
Wait-LogLine 'beidebug: give minecraft:wool:14 2 -> true' 20 | Out-Null
Open-Inventory
Save-Screenshot 'give' $u | Out-Null
Close-Screen

# --- 2x2 transfer in the inventory -----------------------------------------------------------
Invoke-Chat '/beidebug transfer planks 2'   # count > 1: as many sets as possible (5 logs)
Open-Inventory
Wait-LogLine 'beidebug: transfer beidebug:planks max=true -> true' 20 | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'transfer_planks' $u | Out-Null
Close-Screen

Invoke-Chat '/beidebug check chest'
Open-Inventory
Wait-LogLine 'beidebug: grid=2 check beidebug:chest: possible=false' 20 | Out-Null
Close-Screen

Invoke-Chat '/beidebug transfer table'
Open-Inventory
Wait-LogLine 'beidebug: transfer beidebug:table max=false -> true' 20 | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'transfer_table' $u | Out-Null
Close-Screen

# --- 3x3 transfer in a crafting table: stand on one and look straight down ---------------------
Invoke-Chat '/setblock ~ ~-1 ~ crafting_table'
Invoke-Chat '/tp @s ~ ~ ~ 0 90'
Start-Sleep -Milliseconds 800
Invoke-Chat '/beidebug transfer chest'
Focus-Game
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'beidebug: grid=3 check beidebug:chest: possible=true' 20 | Out-Null
Wait-LogLine 'beidebug: transfer beidebug:chest max=false -> true' 10 | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'transfer_chest' $u | Out-Null
Close-Screen

# --- delete the cursor item (creative only) --------------------------------------------------
Invoke-Chat '/gamemode creative'
Invoke-Chat '/beidebug delete'
Open-Inventory
Save-Screenshot 'creative_inventory' $u | Out-Null
Click 0.399 0.803 -Relative   # creative hotbar slot 2 (oak planks): picks the stack up onto the cursor
Wait-LogLine 'beidebug: delete -> ' 20 | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'delete' $u | Out-Null
Close-Screen
Invoke-Chat '/gamemode survival'
Wait-LogLine 'beidebug: delete -> true \(cursor now empty\)' 5 | Out-Null
