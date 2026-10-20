# Item list overlay + bookmark panel: layout next to the inventory, hover tooltips, paging by
# the ">" button and the mouse wheel, search box focus. Bookmarks are seeded by
# overlay.prelaunch.ps1. Screenshots: out/overlay/*.png
$u = 'overlay'

# Parse "<name> layout: ..." (fractions of the client area) logged by the panels.
function Get-PanelLayout([string]$name) {
    $line = Wait-LogLine "$name layout:" 30
    $l = @{}
    foreach ($m in [regex]::Matches($line, '(\w+)=([-\d.]+)(?:,([-\d.]+))?')) {
        $l[$m.Groups[1].Value] = if ($m.Groups[3].Success) { @([double]$m.Groups[2].Value, [double]$m.Groups[3].Value) } else { [double]$m.Groups[2].Value }
    }
    Write-Step "$name layout: $line"
    return $l
}
function Cell-Pos($l, [int]$col, [int]$row) {
    return @(($l.cell0[0] + $col * $l.cell[0]), ($l.cell0[1] + $row * $l.cell[1]))
}

try {
    Wait-LogLine 'Recipe index rebuilt' 60 | Out-Null
    Open-Inventory
    Start-Sleep -Milliseconds 500
    $items = Get-PanelLayout 'Item panel'
    $books = Get-PanelLayout 'Bookmark panel'
    Move-Mouse 0.5 0.95 -Relative   # park the cursor away from the panels
    Start-Sleep -Milliseconds 300
    Save-Screenshot 'inventory' $u | Out-Null

    $p = Cell-Pos $items 0 0
    Move-Mouse $p[0] $p[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'hover_first' $u | Out-Null
    $p = Cell-Pos $items ($items.cols - 1) 3
    Move-Mouse $p[0] $p[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'hover_right_edge' $u | Out-Null

    # ">" button: page 2.
    Click $items.next[0] $items.next[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'page2_button' $u | Out-Null
    # Wheel down over the grid: page 3; wheel up: back to page 2.
    $p = Cell-Pos $items 1 1
    Move-Mouse $p[0] $p[1] -Relative; Start-Sleep -Milliseconds 200
    Scroll -1; Start-Sleep -Milliseconds 400
    Save-Screenshot 'page3_wheel' $u | Out-Null
    Scroll 1; Start-Sleep -Milliseconds 400
    Save-Screenshot 'page2_wheel_back' $u | Out-Null
    # "<" twice wraps from page 2 to the last page... first back to page 1.
    Click $items.prev[0] $items.prev[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'page1_button' $u | Out-Null

    # Bookmark panel hover.
    $p = Cell-Pos $books 1 0
    Move-Mouse $p[0] $p[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'hover_bookmark' $u | Out-Null

    # Search box focus (typing needs the input unit's char hook).
    Click $items.extra[0] $items.extra[1] -Relative; Start-Sleep -Milliseconds 400
    Save-Screenshot 'search_focused' $u | Out-Null
    Press-Key ESC   # unfocus the box (swallowed by BEI while focused)
    Close-Screen
    Start-Sleep -Milliseconds 400
    Save-Screenshot 'closed' $u | Out-Null
} finally {
    # Restore the user's config (overlay.prelaunch.ps1 made a backup).
    if ($script:OverlayBackup) {
        foreach ($f in $script:OverlayBackup.Keys) {
            $path = Join-Path $script:OverlayCfgDir $f
            if ($null -eq $script:OverlayBackup[$f]) { Remove-Item $path -ErrorAction SilentlyContinue }
            else { [IO.File]::WriteAllText($path, $script:OverlayBackup[$f]) }
        }
        Write-Step "restored BEI config"
    }
}
