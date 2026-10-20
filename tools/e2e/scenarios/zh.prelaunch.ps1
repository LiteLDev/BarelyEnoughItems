# Chinese game language + BEI cheat mode for the zh scenario. Originals are backed up next to
# the files (*.bei-e2e-bak) and restored by zh.restore.ps1.
$opts = Get-ChildItem (Get-GameDataDir) -Recurse -Filter options.txt -ErrorAction SilentlyContinue | Select-Object -First 1
if ($opts) {
    Copy-Item $opts.FullName "$($opts.FullName).bei-e2e-bak" -Force
    (Get-Content $opts.FullName) -replace '^game_language:.*$', 'game_language:zh_CN' | Set-Content $opts.FullName -Encoding UTF8
}
$cfg = Join-Path $script:ModsDir 'BEI\config\config.json'
if (Test-Path $cfg) {
    Copy-Item $cfg "$cfg.bei-e2e-bak" -Force
    $j = Get-Content $cfg -Raw | ConvertFrom-Json
    $j.cheatMode = $true
    $j.lastSearch = ''
    $j | ConvertTo-Json | Set-Content $cfg -Encoding UTF8
}
