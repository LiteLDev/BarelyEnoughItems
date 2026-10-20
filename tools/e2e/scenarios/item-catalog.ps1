# Item catalog: the catalog is built (creative order + registry-only items) with timing, and the
# item panel shows names/icons for creative items and for registry-only items found via search.
# Screenshots: out/item-catalog/{page1,hover,page2,search-<query>,search-<query>-hover}.png
$u = 'item-catalog'
$line = Wait-LogLine 'Item catalog: \d+ items' 60
Write-Host "catalog: $line"
Open-Inventory
Save-Screenshot 'page1' $u | Out-Null
Move-Mouse 0.836 0.083 -Relative
Start-Sleep -Milliseconds 500
Save-Screenshot 'hover' $u | Out-Null
Move-Mouse 0.9 0.4 -Relative
Scroll -1
Save-Screenshot 'page2' $u | Out-Null

# Registry-only items (not in the creative inventory) via the search box.
foreach ($q in @('command', 'light', 'spawn egg', 'barrier')) {
    Click 0.9 0.957 -Relative
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt 12; $i++) { Press-Key BACK 20 }
    Type-Text $q
    Start-Sleep -Milliseconds 400
    $name = 'search-' + ($q -replace ' ', '-')
    Save-Screenshot $name $u | Out-Null
    Move-Mouse 0.836 0.083 -Relative
    Start-Sleep -Milliseconds 400
    Save-Screenshot "$name-hover" $u | Out-Null
}
Click 0.9 0.957 -Relative
for ($i = 0; $i -lt 12; $i++) { Press-Key BACK 20 }
Close-Screen
