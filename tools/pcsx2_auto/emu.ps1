param([string]$cmd, [string]$arg1, [string]$arg2)
$ini = "$env:USERPROFILE\Documents\PCSX2\inis\PCSX2.ini"
$bak = "$PSScriptRoot\PCSX2.ini.bak"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { 
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint fl, UIntPtr ex);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
$vk = @{ ENTER=0x0D; SPACE=0x20; X=0x20; C=0x43; V=0x56; Z=0x5A; UP=0x26; DOWN=0x28; LEFT=0x25; RIGHT=0x27; F8=0x77; F1=0x70; PAUSE=0x13 }   # PAUSE = TogglePause do PCSX2
function Get-Emu { Get-Process pcsx2-qt -ErrorAction SilentlyContinue | Select-Object -First 1 }
switch ($cmd) {
 'start' {   # start <elf-name> : launches (the keyboard bindings live permanently in PCSX2.ini; see README)
   $w = 'C:\Users\TwistZero\WoTM'
   $elf = "$w\build\pcsx2_test\SCUS_971.97_$arg1.elf"
   $log = "$w\build\pcsx2_test\log_play_$arg1.txt"
   Remove-Item $log -ErrorAction SilentlyContinue
   Start-Process "D:\Emulators\PCSX2\pcsx2-qt.exe" -WorkingDirectory "D:\Emulators\PCSX2" -ArgumentList @('-batch','-nogui','-logfile',$log,'-elf',$elf,'--',"$w\ISO\SCUS_971.97.War of the Monsters.iso")
   "started $arg1" }
 'snap' {  # snap <file.png> : only the PCSX2 window (PrintWindow), never the desktop
   $p = Get-Emu; if (-not $p) { 'no emulator'; break }
   $r = New-Object W+RECT; [W]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
   $w = $r.R - $r.L; $h = $r.B - $r.T
   if ($w -le 0 -or $h -le 0) { 'window has no size (minimized?)'; break }
   $bmp = New-Object System.Drawing.Bitmap $w, $h
   $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
   [W]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
   $sm = New-Object System.Drawing.Bitmap $bmp, ([int]($w/2)), ([int]($h/2))
   $sm.Save($arg1, [System.Drawing.Imaging.ImageFormat]::Png); "saved window ${w}x${h}" }
 'key' {   # key NAME [holdMs] : posted straight to the PCSX2 window, no focus needed or taken
   $p = Get-Emu; if (-not $p) { 'no emulator'; break }
   $hold = if ($arg2) { [int]$arg2 } else { 150 }
   $k = [uint32]$vk[$arg1.ToUpper()]; $sc = [W]::MapVirtualKey($k, 0)
   $ext = if (@(0x25,0x26,0x27,0x28) -contains $k) { 1 -shl 24 } else { 0 }
   $dn = [IntPtr](1 -bor ($sc -shl 16) -bor $ext)
   $up = [IntPtr]((1 -bor ($sc -shl 16) -bor $ext -bor (1 -shl 30) -bor ([int]0x80000000)))
   [W]::PostMessage($p.MainWindowHandle, 0x100, [IntPtr]$k, $dn) | Out-Null; Start-Sleep -Milliseconds $hold
   [W]::PostMessage($p.MainWindowHandle, 0x101, [IntPtr]$k, $up) | Out-Null; "posted $arg1" }
 'status' { $p = Get-Emu; if ($p) { "running pid $($p.Id)" } else { 'not running' } }
 'stop' {
   Stop-Process -Name pcsx2-qt -Force -ErrorAction SilentlyContinue; Start-Sleep 1; 'stopped' }
}
