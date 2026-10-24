<#
.SYNOPSIS
  Turn the frames and stills captured by the `showcase` e2e scenario into the gallery media in
  docs/showcase/ (WebP stills, H.264 MP4 videos with WebP posters, animated WebP for the README).

.EXAMPLE
  pwsh tools/e2e/run.ps1 -Scenario showcase
  pwsh tools/e2e/showcase-encode.ps1
#>
param(
    [string]$FFmpeg = $(if (Get-Command ffmpeg -ErrorAction SilentlyContinue) { 'ffmpeg' } else {
            "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.2-full_build\bin\ffmpeg.exe" })
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$in = Join-Path $root 'tools\e2e\out\showcase'
$out = Join-Path $root 'docs\showcase'
New-Item -ItemType Directory -Force $out | Out-Null

function ff { & $FFmpeg -hide_banner -loglevel error -y @args; if ($LASTEXITCODE) { throw "ffmpeg failed: $args" } }

# Captures are 1600x900 (GUI scale 3). Gallery size is 1280x720: either a 1:1 crop (crisp) or the
# whole frame scaled with Lanczos.
$fit = 'scale=1280:720:flags=lanczos'
$stills = @(
    @{ name = 'inventory'; vf = $fit }
    @{ name = 'recipes'; vf = $fit }
    @{ name = 'uses'; vf = $fit }
    @{ name = 'brewing'; vf = $fit }
    @{ name = 'bookmarks'; vf = 'crop=1280:720:0:0' }
    @{ name = 'settings'; vf = 'crop=1280:720:320:180' }
)
foreach ($s in $stills) {
    $src = Join-Path $in "$($s.name).png"
    if (-not (Test-Path $src)) { Write-Warning "missing $src"; continue }
    ff -i $src -vf $s.vf '-c:v' libwebp -quality 85 -compression_level 6 (Join-Path $out "$($s.name).webp")
}

# Videos: rec-<name>\frames.txt (frame file + capture time in ms) -> true-speed MP4 (+ WebP poster
# from the frame at `poster` ms) and, for the README, an animated WebP.
$videos = @(
    @{ name = 'search'; poster = 7600; anim = 'crop=800:900:800:0' }
    @{ name = 'transfer'; poster = 6000; anim = 'scale=960:540:flags=lanczos' }
    @{ name = 'cheat'; poster = 7000 }
)
foreach ($v in $videos) {
    $dir = Join-Path $in "rec-$($v.name)"
    $list = Join-Path $dir 'frames.txt'
    if (-not (Test-Path $list)) { Write-Warning "missing $list"; continue }
    $frames = @(Get-Content $list | ForEach-Object { $p = $_ -split ' '; [pscustomobject]@{ f = $p[0]; t = [int]$p[1] } })
    $concat = foreach ($i in 0..($frames.Count - 1)) {
        $d = if ($i -lt $frames.Count - 1) { ($frames[$i + 1].t - $frames[$i].t) / 1000 } else { 0.5 }
        "file '$($frames[$i].f)'"; "duration $d"
    }
    $concat += "file '$($frames[-1].f)'"
    $concatFile = Join-Path $dir 'concat.txt'
    Set-Content $concatFile $concat -Encoding ascii
    ff -f concat -safe 0 -i $concatFile -t 11.9 -vf "fps=30,$fit,format=yuv420p" '-c:v' libx264 -preset slow -crf 26 -an -movflags +faststart (Join-Path $out "$($v.name).mp4")
    $posterFrame = ($frames | Where-Object { $_.t -le $v.poster } | Select-Object -Last 1).f
    ff -i (Join-Path $dir $posterFrame) -vf $fit '-c:v' libwebp -quality 85 (Join-Path $out "$($v.name)-poster.webp")
    if ($v.anim) {
        ff -f concat -safe 0 -i $concatFile -t 11.9 -vf "fps=12,$($v.anim)" '-c:v' libwebp_anim -quality 70 -compression_level 6 -loop 0 (Join-Path $out "$($v.name).webp")
    }
}
Get-ChildItem $out | Select-Object Name, @{ n = 'KB'; e = { [int]($_.Length / 1KB) } } | Format-Table -AutoSize
