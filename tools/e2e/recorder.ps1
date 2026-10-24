# Frame recorder for showcase videos: grabs the game window (PrintWindow) in a background thread,
# crops it to the client area, draws the mouse cursor (PrintWindow does not capture it) and writes
# numbered BMP frames plus frames.txt ("<file> <ms since start>") for true-speed encoding.
#
#   . "$PSScriptRoot/recorder.ps1"
#   Start-Recording 'search'          # frames go to out\<unit>\rec-search\
#   ... drive the game ...
#   Stop-Recording                    # returns the frame count and the measured fps
#
# tools/e2e/showcase-encode.ps1 turns the frames into MP4 / animated WebP.

if (-not ('BeiE2E.Recorder' -as [type])) {
    # Plain GDI (no System.Drawing): PrintWindow into a DIB section, draw the cursor, write the
    # client-area rectangle as a 24-bit BMP.
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
namespace BeiE2E {
public static class Recorder {
    [StructLayout(LayoutKind.Sequential)] struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct CURSORINFO { public int cbSize, flags; public IntPtr hCursor; public POINT pt; }
    [StructLayout(LayoutKind.Sequential)] struct ICONINFO { public bool fIcon; public int xHotspot, yHotspot; public IntPtr hbmMask, hbmColor; }
    [StructLayout(LayoutKind.Sequential)] struct BITMAPINFOHEADER {
        public int biSize, biWidth, biHeight; public short biPlanes, biBitCount;
        public int biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant; }
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] static extern bool GetCursorInfo(ref CURSORINFO ci);
    [DllImport("user32.dll")] static extern bool GetIconInfo(IntPtr h, out ICONINFO ii);
    [DllImport("user32.dll")] static extern bool DrawIconEx(IntPtr hdc, int x, int y, IntPtr h, int w, int hgt, int step, IntPtr brush, int flags);
    [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
    [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr h);
    [DllImport("user32.dll")] static extern IntPtr LoadCursor(IntPtr inst, int id);
    [DllImport("gdi32.dll")] static extern IntPtr CreateDIBSection(IntPtr dc, ref BITMAPINFOHEADER bi, uint usage, out IntPtr bits, IntPtr section, uint offset);

    static Thread thread;
    static volatile bool stop;
    static List<string> lines;
    public static int Frames;
    public static double Seconds;
    // Headless runs: the game's cursor is not the OS cursor. The harness reports where it put the
    // game's mouse (client pixels) and the recorder draws the standard arrow there.
    public static volatile bool UseVirtual;
    public static volatile int VirtX = -1, VirtY = -1;

    public static void Start(IntPtr hwnd, string dir, bool cursor) {
        Directory.CreateDirectory(dir);
        stop = false; Frames = 0; Seconds = 0; lines = new List<string>();
        thread = new Thread(() => Loop(hwnd, dir, cursor)) { IsBackground = true, Priority = ThreadPriority.AboveNormal };
        thread.Start();
    }
    public static void Stop(string dir) {
        stop = true;
        if (thread != null) thread.Join();
        File.WriteAllLines(Path.Combine(dir, "frames.txt"), lines);
    }
    static void Loop(IntPtr hwnd, string dir, bool cursor) {
        var sw = Stopwatch.StartNew();
        IntPtr screen = GetDC(IntPtr.Zero);
        IntPtr mem = CreateCompatibleDC(screen);
        IntPtr dib = IntPtr.Zero, bits = IntPtr.Zero, old = IntPtr.Zero;
        int dw = 0, dh = 0;
        try {
            while (!stop) {
                long t = sw.ElapsedMilliseconds;
                RECT wr; GetWindowRect(hwnd, out wr);
                RECT cr; GetClientRect(hwnd, out cr);
                POINT org = new POINT(); ClientToScreen(hwnd, ref org);
                int ww = wr.Right - wr.Left, wh = wr.Bottom - wr.Top, cw = cr.Right, ch = cr.Bottom;
                if (ww <= 0 || wh <= 0 || cw <= 0 || ch <= 0) { Thread.Sleep(10); continue; }
                if (ww != dw || wh != dh) {
                    if (dib != IntPtr.Zero) { SelectObject(mem, old); DeleteObject(dib); }
                    var bi = new BITMAPINFOHEADER { biSize = 40, biWidth = ww, biHeight = -wh, biPlanes = 1, biBitCount = 32 };
                    dib = CreateDIBSection(screen, ref bi, 0, out bits, IntPtr.Zero, 0);
                    old = SelectObject(mem, dib); dw = ww; dh = wh;
                }
                PrintWindow(hwnd, mem, 2);
                int ox = org.X - wr.Left, oy = org.Y - wr.Top;
                if (cursor && UseVirtual) {
                    int vx = VirtX, vy = VirtY;
                    if (vx >= 0 && vy >= 0) DrawIconEx(mem, ox + vx, oy + vy, LoadCursor(IntPtr.Zero, 32512), 0, 0, 0, IntPtr.Zero, 3);
                } else if (cursor) {
                    var ci = new CURSORINFO(); ci.cbSize = Marshal.SizeOf(typeof(CURSORINFO));
                    if (GetCursorInfo(ref ci) && (ci.flags & 1) != 0 && ci.hCursor != IntPtr.Zero) {
                        ICONINFO ii; int hx = 0, hy = 0;
                        if (GetIconInfo(ci.hCursor, out ii)) {
                            hx = ii.xHotspot; hy = ii.yHotspot;
                            if (ii.hbmMask != IntPtr.Zero) DeleteObject(ii.hbmMask);
                            if (ii.hbmColor != IntPtr.Zero) DeleteObject(ii.hbmColor);
                        }
                        DrawIconEx(mem, ci.pt.X - wr.Left - hx, ci.pt.Y - wr.Top - hy, ci.hCursor, 0, 0, 0, IntPtr.Zero, 3);
                    }
                }
                // 24-bit bottom-up BMP of the client rectangle.
                int stride = (cw * 3 + 3) & ~3;
                byte[] file = new byte[54 + stride * ch];
                byte[] row = new byte[ww * 4];
                Action<int, int> put = (o, v) => { file[o] = (byte)v; file[o+1] = (byte)(v >> 8); file[o+2] = (byte)(v >> 16); file[o+3] = (byte)(v >> 24); };
                file[0] = (byte)'B'; file[1] = (byte)'M'; put(2, file.Length); put(10, 54); put(14, 40);
                put(18, cw); put(22, ch); file[26] = 1; file[28] = 24; put(34, stride * ch);
                for (int y = 0; y < ch; y++) {
                    int sy = oy + y; if (sy < 0 || sy >= wh) continue;
                    Marshal.Copy(IntPtr.Add(bits, sy * ww * 4), row, 0, ww * 4);
                    int dst = 54 + (ch - 1 - y) * stride;
                    for (int x = 0; x < cw; x++) {
                        int sx = (ox + x) * 4; if (sx < 0 || sx + 3 >= row.Length) continue;
                        file[dst + x * 3] = row[sx]; file[dst + x * 3 + 1] = row[sx + 1]; file[dst + x * 3 + 2] = row[sx + 2];
                    }
                }
                string name = string.Format("f{0:D5}.bmp", Frames);
                File.WriteAllBytes(Path.Combine(dir, name), file);
                lines.Add(name + " " + t);
                Frames++;
                Seconds = sw.Elapsed.TotalSeconds;
            }
        } finally {
            if (dib != IntPtr.Zero) { SelectObject(mem, old); DeleteObject(dib); }
            DeleteDC(mem); ReleaseDC(IntPtr.Zero, screen);
        }
    }
}
}
'@
}

$script:RecDir = $null
[BeiE2E.Recorder]::UseVirtual = [bool]$script:Headless
if ($script:Headless) {
    # Track where the game's (virtual) mouse is, for the cursor drawn into the frames.
    $script:RecOriginalMoveMouse = ${function:Move-Mouse}
    function Move-Mouse([double]$x, [double]$y, [switch]$Relative) {
        $c = Get-ClientRect
        if ($Relative) { $px = $x * $c.W; $py = $y * $c.H } else { $px = $x; $py = $y }
        & $script:RecOriginalMoveMouse $x $y -Relative:$Relative
        [BeiE2E.Recorder]::VirtX = [int]$px; [BeiE2E.Recorder]::VirtY = [int]$py
    }
}
function Start-Recording([string]$name, [switch]$NoCursor) {
    $script:RecDir = Join-Path $script:OutDir "$script:Unit\rec-$name"
    if (Test-Path $script:RecDir) { Remove-Item -Recurse -Force $script:RecDir }
    [BeiE2E.Recorder]::Start((Get-GameWindow), $script:RecDir, -not $NoCursor)
    Start-Sleep -Milliseconds 300
    Write-Step "recording $name -> $script:RecDir"
}
function Stop-Recording {
    [BeiE2E.Recorder]::Stop($script:RecDir)
    $n = [BeiE2E.Recorder]::Frames; $s = [BeiE2E.Recorder]::Seconds
    Write-Step ("recorded {0} frames in {1:N1} s ({2:N1} fps)" -f $n, $s, ($n / [Math]::Max($s, 0.001)))
}

# Full-resolution, lossless still of the client area (Save-Screenshot downscales to 1280 wide).
function Save-Still([string]$name) {
    $dir = Join-Path $script:OutDir "$script:Unit\still-$name"
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    [BeiE2E.Recorder]::Start((Get-GameWindow), $dir, $false)
    while ([BeiE2E.Recorder]::Frames -lt 1) { Start-Sleep -Milliseconds 20 }
    [BeiE2E.Recorder]::Stop($dir)
    $png = Join-Path $script:OutDir "$script:Unit\$name.png"
    $bmp = [System.Drawing.Image]::FromFile((Join-Path $dir 'f00000.bmp'))
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }
    Remove-Item -Recurse -Force $dir
    Write-Step "still $png"
}
