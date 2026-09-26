param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [Parameter(Mandatory = $true)][string]$OutFile,
    [int]$Seconds = 80,
    [int]$Fps = 30,
    [string]$StampFile = ""
)
# Records a window's client area with PrintWindow (works while covered, never steals focus),
# piping raw BGRA frames into ffmpeg. Frames are paced by the wall clock (a late capture is
# duplicated) so the video stays in real time; the UTC time of frame 0 goes to $StampFile.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
public static class WinRec {
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    public static long Run(IntPtr hwnd, string outFile, int seconds, int fps, int cx, int cy, int cw, int ch, string stampFile) {
        SetProcessDPIAware();
        var psi = new ProcessStartInfo("ffmpeg", string.Format(
            "-y -loglevel error -f rawvideo -pix_fmt bgr0 -s {0}x{1} -r {2} -i - -c:v libx264 -preset veryfast -crf 12 -pix_fmt yuv420p \"{3}\"",
            cw, ch, fps, outFile));
        psi.UseShellExecute = false; psi.RedirectStandardInput = true;
        // Process.StandardInput is a StreamWriter over Console.InputEncoding; a UTF-8 BOM there would
        // prefix 3 bytes to the raw frames and shift every pixel's channels.
        Console.InputEncoding = new System.Text.UTF8Encoding(false);
        var ff = Process.Start(psi);
        var stdin = ff.StandardInput.BaseStream;
        RECT r; GetWindowRect(hwnd, out r);
        var bmp = new Bitmap(r.Right - r.Left, r.Bottom - r.Top, PixelFormat.Format32bppRgb);
        var frame = new byte[cw * ch * 4];
        long total = (long)seconds * fps, written = 0;
        Stopwatch sw = null;
        while (written < total && IsWindow(hwnd)) {
            using (var g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc(); PrintWindow(hwnd, hdc, 2); g.ReleaseHdc(hdc);
            }
            var data = bmp.LockBits(new Rectangle(cx, cy, cw, ch), ImageLockMode.ReadOnly, PixelFormat.Format32bppRgb);
            for (int y = 0; y < ch; y++) Marshal.Copy(data.Scan0 + y * data.Stride, frame, y * cw * 4, cw * 4);
            bmp.UnlockBits(data);
            if (sw == null) {
                sw = Stopwatch.StartNew();
                if (stampFile.Length > 0) File.WriteAllText(stampFile, DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss.fff"));
            }
            long due = (long)(sw.Elapsed.TotalSeconds * fps) + 1;
            while (written < due && written < total) { stdin.Write(frame, 0, frame.Length); written++; }
            double wait = (double)written / fps - sw.Elapsed.TotalSeconds;
            if (wait > 0) Thread.Sleep((int)(wait * 1000));
        }
        stdin.Close(); ff.WaitForExit();
        return written;
    }
}
"@
$p = Get-Process -Id $ProcessId
while ($p.MainWindowHandle -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 200; $p.Refresh() }
$frames = [WinRec]::Run($p.MainWindowHandle, $OutFile, $Seconds, $Fps, 8, 31, 440, 950, $StampFile)
Write-Output "frames $frames"
