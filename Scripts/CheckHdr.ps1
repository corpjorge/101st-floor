<#
.SYNOPSIS
    Reads the connected displays' DXGI signal/luminance and EDID HDR capabilities without changing Windows settings.
.EXAMPLE
    .\Scripts\CheckHdr.ps1
#>
[CmdletBinding()]
param([string]$ReportPath = "$PSScriptRoot\..\Saved\HDR\Monitor.json")

$ErrorActionPreference = 'Stop'
if (-not ('Floor101.DisplayProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
namespace Floor101 {
    public static class DisplayProbe {
        [StructLayout(LayoutKind.Sequential)] public struct Luid { public uint Low; public int High; }
        [StructLayout(LayoutKind.Sequential)] public struct Source { public Luid Adapter; public uint Id, Mode, Status; }
        [StructLayout(LayoutKind.Sequential)] public struct Rational { public uint Numerator, Denominator; }
        [StructLayout(LayoutKind.Sequential)] public struct Target {
            public Luid Adapter; public uint Id, Mode, Technology, Rotation, Scaling;
            public Rational Refresh; public uint Scanline; public int Available; public uint Status;
        }
        [StructLayout(LayoutKind.Sequential)] public struct DisplayPath { public Source Source; public Target Target; public uint Flags; }
        [StructLayout(LayoutKind.Explicit, Size = 64)] public struct ModeInfo { [FieldOffset(0)] public uint InfoType; }
        [StructLayout(LayoutKind.Sequential)] public struct Header { public uint Type, Size; public Luid Adapter; public uint Id; }
        [StructLayout(LayoutKind.Sequential)] public struct ColorInfo { public Header Header; public uint Value, Encoding, Bits, ActiveMode; }
        [StructLayout(LayoutKind.Sequential)] public struct HdrState { public Header Header; public uint Enabled; }
        public class WindowsState {
            public Header Header;
            public bool Supported, Enabled, Active;
            public uint BitsPerColor;
        }
        [DllImport("user32.dll")] static extern int GetDisplayConfigBufferSizes(uint flags, out uint paths, out uint modes);
        [DllImport("user32.dll")] static extern int QueryDisplayConfig(uint flags, ref uint paths, [Out] DisplayPath[] pathInfo,
            ref uint modes, [Out] ModeInfo[] modeInfo, IntPtr topology);
        [DllImport("user32.dll")] static extern int DisplayConfigGetDeviceInfo(ref ColorInfo info);
        [DllImport("user32.dll")] static extern int DisplayConfigSetDeviceInfo(ref HdrState state);
        public static WindowsState[] ReadWindows() {
            for (int attempt = 0; attempt < 3; attempt++) {
                uint paths, modes;
                int error = GetDisplayConfigBufferSizes(2, out paths, out modes);
                if (error != 0) throw new System.ComponentModel.Win32Exception(error);
                var pathInfo = new DisplayPath[paths]; var modeInfo = new ModeInfo[modes];
                error = QueryDisplayConfig(2, ref paths, pathInfo, ref modes, modeInfo, IntPtr.Zero);
                if (error == 122) continue;
                if (error != 0) throw new System.ComponentModel.Win32Exception(error);
                var states = new List<WindowsState>();
                for (int i = 0; i < paths; i++) {
                    var info = new ColorInfo { Header = new Header { Type = 15, Size = 36, Adapter = pathInfo[i].Target.Adapter, Id = pathInfo[i].Target.Id } };
                    error = DisplayConfigGetDeviceInfo(ref info);
                    if (error != 0) throw new System.ComponentModel.Win32Exception(error);
                    states.Add(new WindowsState { Header = info.Header, Supported = (info.Value & 16) != 0,
                        Enabled = (info.Value & 32) != 0, Active = info.ActiveMode == 2, BitsPerColor = info.Bits });
                }
                return states.ToArray();
            }
            throw new InvalidOperationException("Display topology kept changing; retry the HDR probe.");
        }
        // Only used by the opt-in runtime test, which restores Windows HDR in finally.
        public static void SetWindows(WindowsState display, bool enabled) {
            var header = display.Header; header.Type = 16; header.Size = 24;
            var state = new HdrState { Header = header, Enabled = enabled ? 1u : 0u };
            int error = DisplayConfigSetDeviceInfo(ref state);
            if (error != 0) throw new System.ComponentModel.Win32Exception(error);
        }
        [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] public struct Output {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
            public Rect DesktopCoordinates;
            public int AttachedToDesktop, Rotation;
            public IntPtr Monitor;
            public uint BitsPerColor, ColorSpace;
            public float RedX, RedY, GreenX, GreenY, BlueX, BlueY, WhiteX, WhiteY;
            public float MinLuminance, MaxLuminance, MaxFullFrameLuminance;
        }
        [DllImport("dxgi.dll")] static extern int CreateDXGIFactory1(ref Guid iid, out IntPtr factory);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Enumerate(IntPtr self, uint index, out IntPtr value);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Description(IntPtr self, out Output value);
        static T Method<T>(IntPtr obj, int slot) where T : Delegate {
            return Marshal.GetDelegateForFunctionPointer<T>(Marshal.ReadIntPtr(Marshal.ReadIntPtr(obj), slot * IntPtr.Size));
        }
        public static Output[] Read() {
            var values = new List<Output>();
            var iid = new Guid("770aae78-f26f-4dba-a829-253c83d1b387");
            IntPtr factory;
            Marshal.ThrowExceptionForHR(CreateDXGIFactory1(ref iid, out factory));
            try {
                for (uint a = 0; ; a++) {
                    IntPtr adapter;
                    int hr = Method<Enumerate>(factory, 12)(factory, a, out adapter);
                    if (hr == unchecked((int)0x887A0002)) break;
                    Marshal.ThrowExceptionForHR(hr);
                    try {
                        for (uint o = 0; ; o++) {
                            IntPtr output;
                            hr = Method<Enumerate>(adapter, 7)(adapter, o, out output);
                            if (hr == unchecked((int)0x887A0002)) break;
                            Marshal.ThrowExceptionForHR(hr);
                            try {
                                IntPtr output6;
                                var outputId = new Guid("068346e8-aaec-4b84-add7-137f513f77a1");
                                Marshal.ThrowExceptionForHR(Marshal.QueryInterface(output, in outputId, out output6));
                                try {
                                    Output desc;
                                    Marshal.ThrowExceptionForHR(Method<Description>(output6, 27)(output6, out desc));
                                    if (desc.AttachedToDesktop != 0) values.Add(desc);
                                } finally { Marshal.Release(output6); }
                            } finally { Marshal.Release(output); }
                        }
                    } finally { Marshal.Release(adapter); }
                }
            } finally { Marshal.Release(factory); }
            return values.ToArray();
        }
    }
}
'@
}

$signals = @([Floor101.DisplayProbe]::Read() | ForEach-Object {
    [pscustomobject]@{
        Device = $_.DeviceName
        Resolution = '{0}x{1}' -f ($_.DesktopCoordinates.Right - $_.DesktopCoordinates.Left), ($_.DesktopCoordinates.Bottom - $_.DesktopCoordinates.Top)
        BitsPerColor = $_.BitsPerColor
        ColorSpace = $_.ColorSpace
        WindowsHdrActive = $_.ColorSpace -eq 12 # DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        MinNits = $_.MinLuminance
        PeakNits = $_.MaxLuminance
        FullFrameNits = $_.MaxFullFrameLuminance
    }
})
$monitors = @(Get-CimInstance -Namespace root\wmi -Class WmiMonitorID | Where-Object Active | ForEach-Object {
    $identity = $_
    $descriptor = Get-CimInstance -Namespace root\wmi -Class WmiMonitorDescriptorMethods |
        Where-Object InstanceName -eq $identity.InstanceName
    $pq = $false; $hlg = $false; $staticType1 = $false
    if ($descriptor) {
        $base = (Invoke-CimMethod -InputObject $descriptor -MethodName WmiGetMonitorRawEEdidV1Block -Arguments @{BlockId=[byte]0}).BlockContent
        for ($block = 1; $block -le $base[126]; $block++) {
            $bytes = (Invoke-CimMethod -InputObject $descriptor -MethodName WmiGetMonitorRawEEdidV1Block -Arguments @{BlockId=[byte]$block}).BlockContent
            if ($bytes[0] -ne 2) { continue } # CTA-861 extension
            $end = if ($bytes[2] -eq 0) { 127 } else { [int]$bytes[2] }
            for ($i = 4; $i -lt $end;) {
                $length = $bytes[$i] -band 31
                if ($i + $length -ge $end) { break }
                if (($bytes[$i] -shr 5) -eq 7 -and $length -ge 3 -and $bytes[$i+1] -eq 6) {
                    $pq = $pq -or (($bytes[$i+2] -band 4) -ne 0)
                    $hlg = $hlg -or (($bytes[$i+2] -band 8) -ne 0)
                    $staticType1 = $staticType1 -or (($bytes[$i+3] -band 1) -ne 0)
                }
                $i += $length + 1
            }
        }
    }
    [pscustomobject]@{
        Model = -join ($identity.UserFriendlyName | Where-Object {$_} | ForEach-Object {[char]$_})
        EdidPQ = $pq; EdidHLG = $hlg; EdidStaticMetadataType1 = $staticType1
        EdidRead = [bool]$descriptor
    }
})
$windows = @([Floor101.DisplayProbe]::ReadWindows() | Select-Object Supported,Enabled,Active,BitsPerColor)
$report = [pscustomobject]@{ CapturedAt = (Get-Date).ToString('o'); Signals = $signals; Monitors = $monitors; Windows = $windows }
New-Item -ItemType Directory -Force (Split-Path $ReportPath) | Out-Null
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $ReportPath -Encoding utf8
$report | ConvertTo-Json -Depth 5
