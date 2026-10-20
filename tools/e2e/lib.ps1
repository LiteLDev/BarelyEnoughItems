# BEI end-to-end test helpers: deploy the mod, drive Minecraft Bedrock through LeviLauncher,
# send real input (SendInput) and capture screenshots.
#
# Dot-source this file:  . "$PSScriptRoot/lib.ps1"
# All coordinates passed to Click/Move are *client-area pixels* of the game window, or
# fractions (0..1) of the client area when -Relative is used.


$script:RepoRoot     = (Resolve-Path "$PSScriptRoot/../..").Path
$script:Launcher     = Join-Path $env:USERPROFILE 'Desktop\LeviLauncher.exe'
$script:LauncherData = Join-Path $env:APPDATA 'levilauncher.exe'
$script:VersionName  = '1.26.51.01'
$script:VersionDir   = Join-Path $script:LauncherData "versions\$script:VersionName"
$script:ModsDir      = Join-Path $script:VersionDir 'mods'
$script:GameExe      = Join-Path $script:VersionDir 'Minecraft.Windows.exe'
$script:LockFile     = Join-Path $script:LauncherData 'bei-e2e.lock'
$script:OutDir       = Join-Path $script:RepoRoot 'tools\e2e\out'

if (-not ('BeiE2E.Native' -as [type])) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace BeiE2E {
public static class Native {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)] struct UNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public UNION u; }

    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    public static void ForceForeground(IntPtr h) {
        IntPtr fg = GetForegroundWindow();
        uint fgThread = GetWindowThreadProcessId(fg, IntPtr.Zero);
        uint me = GetCurrentThreadId();
        AttachThreadInput(me, fgThread, true);
        keybd_event(0x12, 0, 0, IntPtr.Zero); keybd_event(0x12, 0, 2, IntPtr.Zero);
        ShowWindow(h, 9);
        BringWindowToTop(h);
        SetForegroundWindow(h);
        AttachThreadInput(me, fgThread, false);
    }

    const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    const uint KEYEVENTF_KEYUP = 2, KEYEVENTF_SCANCODE = 8, KEYEVENTF_UNICODE = 4, KEYEVENTF_EXTENDED = 1;
    const uint MOUSEEVENTF_MOVE = 1, MOUSEEVENTF_LEFTDOWN = 2, MOUSEEVENTF_LEFTUP = 4,
               MOUSEEVENTF_RIGHTDOWN = 8, MOUSEEVENTF_RIGHTUP = 16, MOUSEEVENTF_WHEEL = 0x800;

    static void Send(INPUT i) { SendInput(1, new[] { i }, Marshal.SizeOf(typeof(INPUT))); }

    public static void Key(ushort vk, bool up) {
        var i = new INPUT { type = INPUT_KEYBOARD };
        i.u.ki.wVk = vk;
        i.u.ki.wScan = (ushort)MapVirtualKey(vk, 0);
        i.u.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0);
        if (vk == 0x25 || vk == 0x26 || vk == 0x27 || vk == 0x28 || vk == 0x2E || vk == 0x24 || vk == 0x23) i.u.ki.dwFlags |= KEYEVENTF_EXTENDED;
        Send(i);
    }
    public static void Unicode(char c) {
        var down = new INPUT { type = INPUT_KEYBOARD }; down.u.ki.wScan = c; down.u.ki.dwFlags = KEYEVENTF_UNICODE;
        var up = new INPUT { type = INPUT_KEYBOARD }; up.u.ki.wScan = c; up.u.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        Send(down); Send(up);
    }
    public static void MouseButton(int button, bool up) {
        var i = new INPUT { type = INPUT_MOUSE };
        if (button == 0) i.u.mi.dwFlags = up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
        else i.u.mi.dwFlags = up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
        Send(i);
    }
    public static void Wheel(int delta) {
        var i = new INPUT { type = INPUT_MOUSE }; i.u.mi.dwFlags = MOUSEEVENTF_WHEEL; i.u.mi.mouseData = (uint)delta; Send(i);
    }
    public static void MoveRelative(int dx, int dy) {
        var i = new INPUT { type = INPUT_MOUSE }; i.u.mi.dwFlags = MOUSEEVENTF_MOVE; i.u.mi.dx = dx; i.u.mi.dy = dy; Send(i);
    }
}
}
'@
}

# Physical pixels everywhere (the desktop uses display scaling).
[BeiE2E.Native]::SetProcessDpiAwarenessContext([IntPtr]::new(-4)) | Out-Null

function Write-Step([string]$msg) { Write-Host "[e2e] $msg" }

# ---------------------------------------------------------------------------------------
# Lock (the game instance is shared by every worktree on this machine)
# ---------------------------------------------------------------------------------------
function Enter-E2ELock([int]$TimeoutMinutes = 120) {
    $deadline = (Get-Date).AddMinutes($TimeoutMinutes)
    while ($true) {
        try {
            $script:LockStream = [System.IO.File]::Open($script:LockFile, 'OpenOrCreate', 'ReadWrite', 'None')
            $bytes = [Text.Encoding]::UTF8.GetBytes("pid=$PID repo=$script:RepoRoot at=$(Get-Date -Format o)")
            $script:LockStream.SetLength(0); $script:LockStream.Write($bytes, 0, $bytes.Length); $script:LockStream.Flush()
            Write-Step "lock acquired"
            return
        } catch [System.IO.IOException] {
            if ((Get-Date) -gt $deadline) { throw "Timed out waiting for e2e lock $script:LockFile" }
            Start-Sleep -Seconds 15
        }
    }
}

function Exit-E2ELock {
    if ($script:LockStream) { $script:LockStream.Dispose(); $script:LockStream = $null; Write-Step "lock released" }
}

# ---------------------------------------------------------------------------------------
# Deploy / world fixture
# ---------------------------------------------------------------------------------------
function Get-GameDataDir {
    # LeviLauncher isolation: per-version data under the version folder.
    $iso = Join-Path $script:VersionDir 'Minecraft Bedrock'
    if (Test-Path $iso) { return $iso }
    return Join-Path $env:APPDATA 'Minecraft Bedrock'
}

function Get-WorldsDir {
    $data = Get-GameDataDir
    $dirs = @(Get-ChildItem (Join-Path $data 'Users') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'games\com.mojang\minecraftWorlds' } |
        Where-Object { Test-Path $_ })
    # Prefer a real user folder over "Shared".
    $user = $dirs | Where-Object { $_ -notmatch '\\Shared\\' } | Select-Object -First 1
    if ($user) { return $user }
    return $dirs | Select-Object -First 1
}

function Deploy-Mod([string]$BinDir = (Join-Path $script:RepoRoot 'bin\BEI')) {
    if (-not (Test-Path (Join-Path $BinDir 'BEI.dll'))) { throw "Build output not found: $BinDir (run xmake first)" }
    $dest = Join-Path $script:ModsDir 'BEI'
    New-Item -ItemType Directory -Force $dest | Out-Null
    # Keep the user's config/ and data/ directories; replace binaries, manifest and lang.
    Get-ChildItem $dest -File | Remove-Item -Force
    if (Test-Path (Join-Path $dest 'lang')) { Remove-Item -Recurse -Force (Join-Path $dest 'lang') }
    Copy-Item -Recurse -Force (Join-Path $BinDir '*') $dest
    Write-Step "deployed $BinDir -> $dest"
}

function Restore-TestWorld {
    $zip = Join-Path $script:RepoRoot 'tools\e2e\fixtures\BEI-Test.zip'
    if (-not (Test-Path $zip)) { Write-Step "no world fixture, skipping restore"; return }
    $worlds = Get-WorldsDir
    if (-not $worlds) { Write-Step "worlds dir not found (game never started?), skipping restore"; return }
    # Remove every world called BEI-Test (any folder id), then extract a fresh copy.
    Get-ChildItem $worlds -Directory | Where-Object {
        (Get-Content (Join-Path $_.FullName 'levelname.txt') -ErrorAction SilentlyContinue) -eq 'BEI-Test'
    } | Remove-Item -Recurse -Force
    $target = Join-Path $worlds 'BEI-Test'
    Expand-Archive -Path $zip -DestinationPath $target -Force
    Write-Step "restored world fixture to $target"
}

# ---------------------------------------------------------------------------------------
# Game process / window
# ---------------------------------------------------------------------------------------
function Get-GameProcess { Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1 }

function Start-Game([int]$TimeoutSeconds = 180) {
    if (Get-GameProcess) { throw "Minecraft is already running; close it first" }
    # latest.log is appended across sessions: remember where this session starts.
    $script:LogOffset = @(Get-LeviLog -All).Count
    # A leftover launcher instance swallows --launch (single-instance hand-off) without starting
    # the game, so close any running launcher first.
    Get-Process -Name 'LeviLauncher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    Write-Step "launching $script:VersionName via LeviLauncher"
    Start-Process -FilePath $script:Launcher -ArgumentList "--launch=`"$script:VersionName`""
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $p = Get-GameProcess
        if ($p) { $script:Game = $p; Write-Step "game window found (pid $($p.Id))"; return $p }
        Start-Sleep -Seconds 2
    }
    throw "Game window did not appear within $TimeoutSeconds s"
}

function Stop-Game {
    Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | ForEach-Object {
        $_.CloseMainWindow() | Out-Null
        if (-not $_.WaitForExit(10000)) { $_.Kill() }
    }
    Get-Process -Name 'LeviLauncher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Step "game stopped"
}

function Get-GameWindow {
    $p = Get-GameProcess
    if (-not $p) { throw "Minecraft is not running" }
    return $p.MainWindowHandle
}

# Fixed window size so GUI scale and click coordinates are reproducible across runs.
$script:WindowW = 1600
$script:WindowH = 900

function Focus-Game {
    $h = Get-GameWindow
    if ([BeiE2E.Native]::GetForegroundWindow() -eq $h) { return }
    # Keep the game above other windows so synthesized clicks land on it, then click its
    # title bar: a (synthesized) user click is allowed to change the foreground window even
    # when SetForegroundWindow is blocked by the focus-stealing rules.
    [BeiE2E.Native]::SetWindowPos($h, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
    $wr = New-Object BeiE2E.Native+RECT; [BeiE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $c = Get-ClientRect
    if ($c.Y - $wr.Top -gt 4) {
        [BeiE2E.Native]::SetCursorPos([int](($wr.Left + $wr.Right) / 2), [int](($wr.Top + $c.Y) / 2)) | Out-Null
        [BeiE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 50; [BeiE2E.Native]::MouseButton(0, $true)
        Start-Sleep -Milliseconds 300
    }
    for ($i = 0; $i -lt 5; $i++) {
        [BeiE2E.Native]::ForceForeground($h)
        Start-Sleep -Milliseconds 300
        if ([BeiE2E.Native]::GetForegroundWindow() -eq $h) { return }
    }
    Write-Step "warning: could not bring the game window to the foreground"
}

function Set-GameWindowSize([int]$ClientW = $script:WindowW, [int]$ClientH = $script:WindowH) {
    $h = Get-GameWindow
    [BeiE2E.Native]::ShowWindow($h, 9) | Out-Null  # restore (un-maximize)
    $wr = New-Object BeiE2E.Native+RECT; [BeiE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $c = Get-ClientRect
    $extraW = ($wr.Right - $wr.Left) - $c.W; $extraH = ($wr.Bottom - $wr.Top) - $c.H
    [BeiE2E.Native]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, $ClientW + $extraW, $ClientH + $extraH, 0x0040) | Out-Null
    Start-Sleep -Milliseconds 800
}

function Get-ClientRect {
    $h = Get-GameWindow
    $r = New-Object BeiE2E.Native+RECT
    [BeiE2E.Native]::GetClientRect($h, [ref]$r) | Out-Null
    $pt = New-Object BeiE2E.Native+POINT
    [BeiE2E.Native]::ClientToScreen($h, [ref]$pt) | Out-Null
    [pscustomobject]@{ X = $pt.X; Y = $pt.Y; W = $r.Right - $r.Left; H = $r.Bottom - $r.Top }
}

function ConvertTo-Screen([double]$x, [double]$y, [switch]$Relative) {
    $c = Get-ClientRect
    if ($Relative) { $x = $x * $c.W; $y = $y * $c.H }
    [pscustomobject]@{ X = [int]($c.X + $x); Y = [int]($c.Y + $y) }
}

# ---------------------------------------------------------------------------------------
# Input
# ---------------------------------------------------------------------------------------
# Safety: never send synthesized input unless Minecraft is the foreground window, so keys and
# clicks can never land in another application (e.g. a chat window the user is typing in).
function Assert-GameForeground {
    $h = Get-GameWindow
    if ([BeiE2E.Native]::GetForegroundWindow() -ne $h) {
        Focus-Game
        if ([BeiE2E.Native]::GetForegroundWindow() -ne $h) { throw "Minecraft is not the foreground window; refusing to send input" }
    }
}

function Move-Mouse([double]$x, [double]$y, [switch]$Relative) {
    Assert-GameForeground
    $p = ConvertTo-Screen $x $y -Relative:$Relative
    [BeiE2E.Native]::SetCursorPos($p.X, $p.Y) | Out-Null
    # A tiny relative move makes the game register the new position.
    [BeiE2E.Native]::MoveRelative(1, 0); [BeiE2E.Native]::MoveRelative(-1, 0)
    Start-Sleep -Milliseconds 120
}

function Click([double]$x, [double]$y, [switch]$Relative, [switch]$Right) {
    Move-Mouse $x $y -Relative:$Relative
    $b = if ($Right) { 1 } else { 0 }
    [BeiE2E.Native]::MouseButton($b, $false); Start-Sleep -Milliseconds 60; [BeiE2E.Native]::MouseButton($b, $true)
    Start-Sleep -Milliseconds 250
}

function Scroll([int]$notches) { Assert-GameForeground; [BeiE2E.Native]::Wheel(120 * $notches); Start-Sleep -Milliseconds 200 }

$script:VK = @{
    ESC = 0x1B; ENTER = 0x0D; BACK = 0x08; TAB = 0x09; SPACE = 0x20; SHIFT = 0x10; CTRL = 0x11
    LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28; F1 = 0x70; F2 = 0x71; F3 = 0x72
}

function Press-Key([string]$key, [int]$holdMs = 60) {
    Assert-GameForeground
    $vk = if ($script:VK.ContainsKey($key.ToUpper())) { $script:VK[$key.ToUpper()] } else { [int][char]$key.ToUpper() }
    [BeiE2E.Native]::Key([uint16]$vk, $false); Start-Sleep -Milliseconds $holdMs; [BeiE2E.Native]::Key([uint16]$vk, $true)
    Start-Sleep -Milliseconds 200
}

function Type-Text([string]$text) {
    Assert-GameForeground
    foreach ($c in $text.ToCharArray()) { [BeiE2E.Native]::Unicode($c); Start-Sleep -Milliseconds 40 }
    Start-Sleep -Milliseconds 200
}

# ---------------------------------------------------------------------------------------
# Screenshots and logs
# ---------------------------------------------------------------------------------------
function Save-Screenshot([string]$name, [string]$unit = 'scratch') {
    $c = Get-ClientRect
    $dir = Join-Path $script:OutDir $unit
    New-Item -ItemType Directory -Force $dir | Out-Null
    # Capture the window itself (works even when covered by other windows), then crop to
    # the client area.
    $h = Get-GameWindow
    $wr = New-Object BeiE2E.Native+RECT; [BeiE2E.Native]::GetWindowRect($h, [ref]$wr) | Out-Null
    $full = New-Object System.Drawing.Bitmap ($wr.Right - $wr.Left), ($wr.Bottom - $wr.Top)
    $gf = [System.Drawing.Graphics]::FromImage($full)
    $hdc = $gf.GetHdc()
    [BeiE2E.Native]::PrintWindow($h, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
    $gf.ReleaseHdc($hdc); $gf.Dispose()
    $bmp = New-Object System.Drawing.Bitmap $c.W, $c.H
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.DrawImage($full, (New-Object System.Drawing.Rectangle 0, 0, $c.W, $c.H), (New-Object System.Drawing.Rectangle ($c.X - $wr.Left), ($c.Y - $wr.Top), $c.W, $c.H), 'Pixel')
    $full.Dispose()
    # Downscale for review (keeps files small; 1280 wide is plenty to read the UI).
    $scale = [Math]::Min(1.0, 1280.0 / $c.W)
    $out = New-Object System.Drawing.Bitmap ([int]($c.W * $scale)), ([int]($c.H * $scale))
    $g2 = [System.Drawing.Graphics]::FromImage($out)
    $g2.InterpolationMode = 'HighQualityBicubic'
    $g2.DrawImage($bmp, 0, 0, $out.Width, $out.Height)
    $path = Join-Path $dir "$name.png"
    $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $g2.Dispose(); $bmp.Dispose(); $out.Dispose()
    Write-Step "screenshot $path"
    return $path
}

$script:LogOffset = 0
# Lines of LeviLamina's latest.log written by the current game session (-All: whole file).
function Get-LeviLog([switch]$All) {
    $f = Join-Path $script:VersionDir 'logs\latest.log'
    if (-not (Test-Path $f)) { return @() }
    # The game keeps the file open for writing: read with FileShare.ReadWrite.
    $fs = [System.IO.File]::Open($f, 'Open', 'Read', 'ReadWrite')
    try { $text = (New-Object System.IO.StreamReader($fs, [Text.Encoding]::UTF8)).ReadToEnd() } finally { $fs.Dispose() }
    $lines = @($text -split "`r?`n" | Where-Object { $_ -ne '' })
    if ($All) { return $lines }
    if ($script:LogOffset -ge $lines.Count) { return @() }
    return $lines[$script:LogOffset..($lines.Count - 1)]
}

function Wait-LogLine([string]$pattern, [int]$TimeoutSeconds = 120) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $hit = Get-LeviLog | Select-String -Pattern $pattern | Select-Object -Last 1
        if ($hit) { return $hit.Line }
        if (-not (Get-GameProcess)) { throw "Game exited while waiting for log '$pattern'" }
        Start-Sleep -Seconds 2
    }
    throw "Timed out waiting for log line '$pattern'"
}

function Assert-NoErrors {
    $bad = Get-LeviLog | Select-String -Pattern ' (ERR|FTL)\]\[(BEI|LeviLamina|CrashLogger)\]'
    if ($bad) { $bad | ForEach-Object { Write-Host "  $($_.Line)" }; throw "Errors found in LeviLamina log" }
    Write-Step "no BEI errors in log"
}

# ---------------------------------------------------------------------------------------
# Menu navigation (calibrated for a 1600x900 client area; coordinates are fractions)
# ---------------------------------------------------------------------------------------
function Wait-GameReady([int]$TimeoutSeconds = 180) {
    Set-GameWindowSize
    Wait-LogLine 'BEI enabled|Failed to load mod BEI' $TimeoutSeconds | Out-Null
    if (Get-LeviLog | Select-String 'Failed to load mod BEI') { throw "LeviLamina failed to load BEI (see log)" }
    # The title screen takes a while after mods are enabled.
    Start-Sleep -Seconds 15
    Focus-Game
    # First-launch sign-in nag: "Maybe later" (harmless click if absent: empty area).
    Click 0.781 0.775 -Relative
    Start-Sleep -Seconds 2
}

function Enter-TestWorld([int]$LoadSeconds = 30) {
    Focus-Game
    Click 0.498 0.474 -Relative     # Play
    Start-Sleep -Seconds 4
    Click 0.173 0.43 -Relative      # first world thumbnail (BEI-Test, most recently played)
    Start-Sleep -Seconds $LoadSeconds
    Wait-LogLine 'Item catalog: \d+ items' 60 | Out-Null
    Start-Sleep -Seconds 2
    Focus-Game
    Write-Step "in world"
}

# Open the survival inventory (E) and wait for it to render.
function Open-Inventory { Focus-Game; Press-Key E; Start-Sleep -Milliseconds 1200 }
function Close-Screen { Press-Key ESC; Start-Sleep -Milliseconds 800 }

# Run a chat command, e.g. Invoke-Chat '/give @s oak_log 16'
function Invoke-Chat([string]$text) {
    Focus-Game
    Press-Key T; Start-Sleep -Milliseconds 700
    Type-Text $text
    Press-Key ENTER; Start-Sleep -Milliseconds 800
}
