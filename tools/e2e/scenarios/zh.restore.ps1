. "$PSScriptRoot/../lib.ps1"
foreach ($f in @((Get-ChildItem (Get-GameDataDir) -Recurse -Filter 'options.txt.bei-e2e-bak' -ErrorAction SilentlyContinue).FullName, (Join-Path $script:ModsDir 'BEI\config\config.json.bei-e2e-bak'))) {
    if ($f -and (Test-Path $f)) { Move-Item $f ($f -replace '\.bei-e2e-bak$', '') -Force; Write-Step "restored $($f -replace '\.bei-e2e-bak$', '')" }
}
