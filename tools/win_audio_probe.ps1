param(
    [float]$SetScalar = -1.0,
    [ValidateSet("on", "off")]
    [string]$SetMute = "",
    [string]$Id,
    [switch]$SetDefaultDevice,
    [ValidateSet("Capture", "Render")]
    [string]$Flow = "Capture"
)

# CoreAudio helper: enumerate Windows capture endpoints and read/set their
# master scalar volume. Used for UAC gadget volume-mapping diagnostics.
$code = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class AudioProbeUtil
{
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    private class MMDeviceEnumerator { }

    [Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDeviceEnumerator
    {
        int EnumAudioEndpoints(int dataFlow, int stateMask, out IMMDeviceCollection devices);
        int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice endpoint);
        int GetDevice(string pwstrId, out IMMDevice device);
        int RegisterEndpointNotificationCallback(object client);
        int UnregisterEndpointNotificationCallback(object client);
    }

    [Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDeviceCollection
    {
        int GetCount(out int count);
        int Item(int index, out IMMDevice device);
    }

    [Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IMMDevice
    {
        int Activate(ref Guid iid, int clsCtx, IntPtr activationParams, [MarshalAs(UnmanagedType.IUnknown)] out object iface);
        int OpenPropertyStore(int stgmAccess, out IntPtr props);
        int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
        int GetState(out int state);
    }

    [Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IAudioEndpointVolume
    {
        int RegisterControlChangeNotify(IntPtr notify);
        int UnregisterControlChangeNotify(IntPtr notify);
        int GetChannelCount(out int count);
        int SetMasterVolumeLevel(float level, ref Guid ctx);
        int SetMasterVolumeLevelScalar(float level, ref Guid ctx);
        int GetMasterVolumeLevel(out float level);
        int GetMasterVolumeLevelScalar(out float level);
        int SetChannelVolumeLevel(uint channel, float level, ref Guid ctx);
        int SetChannelVolumeLevelScalar(uint channel, float level, ref Guid ctx);
        int GetChannelVolumeLevel(uint channel, out float level);
        int GetChannelVolumeLevelScalar(uint channel, out float level);
        int SetMute(bool mute, ref Guid ctx);
        int GetMute(out bool mute);
        int GetVolumeStepInfo(out uint step, out uint stepCount);
        int VolumeStepUp(ref Guid ctx);
        int VolumeStepDown(ref Guid ctx);
        int QueryHardwareSupport(out uint mask);
        int GetVolumeRange(out float min, out float max, out float increment);
    }

    private static readonly Guid IID_IAudioEndpointVolume = new Guid("5CDF2C82-841E-4546-9722-0CF74078229A");
    private const int CLSCTX_INPROC_SERVER = 1;
    private const int DEVICE_STATE_ACTIVE = 0x1;
    private const int eRender = 0;
    private const int eCapture = 1;

    private static IMMDeviceCollection GetDevices(int flow)
    {
        var en = (IMMDeviceEnumerator)(new MMDeviceEnumerator());
        IMMDeviceCollection coll;
        int hr = en.EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, out coll);
        Marshal.ThrowExceptionForHR(hr);
        return coll;
    }

    private static IAudioEndpointVolume GetVolume(string id)
    {
        var en = (IMMDeviceEnumerator)(new MMDeviceEnumerator());
        IMMDevice dev;
        int hr = en.GetDevice(id, out dev);
        Marshal.ThrowExceptionForHR(hr);
        object o;
        Guid iid = IID_IAudioEndpointVolume;
        hr = dev.Activate(ref iid, CLSCTX_INPROC_SERVER, IntPtr.Zero, out o);
        Marshal.ThrowExceptionForHR(hr);
        return (IAudioEndpointVolume)o;
    }

    public static string[] ListIds(int flow)
    {
        var list = new List<string>();
        var coll = GetDevices(flow);
        int n;
        coll.GetCount(out n);
        for (int i = 0; i < n; i++)
        {
            IMMDevice d;
            coll.Item(i, out d);
            string id;
            d.GetId(out id);
            list.Add(id);
        }
        return list.ToArray();
    }

    public static float GetScalar(string id)
    {
        var v = GetVolume(id);
        float s;
        v.GetMasterVolumeLevelScalar(out s);
        return s;
    }

    public static bool GetMute(string id)
    {
        var v = GetVolume(id);
        bool m;
        v.GetMute(out m);
        return m;
    }

    public static void SetScalar(string id, float scalar)
    {
        var v = GetVolume(id);
        Guid ctx = Guid.Empty;
        v.SetMasterVolumeLevelScalar(scalar, ref ctx);
    }

    public static void SetMute(string id, bool mute)
    {
        var v = GetVolume(id);
        Guid ctx = Guid.Empty;
        v.SetMute(mute, ref ctx);
    }

    [ComImport, Guid("870AF99C-171D-4F9E-AF0D-E63DF40C2BC9")]
    private class CPolicyConfigClient { }

    [Guid("F8679F50-850A-41CF-9C72-430F290290C8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IPolicyConfig
    {
        int GetMixFormat(string pszDeviceName, IntPtr ppFormat);
        int GetDeviceFormat(string pszDeviceName, int bDefault, IntPtr ppFormat);
        int ResetDeviceFormat(string pszDeviceName);
        int SetDeviceFormat(string pszDeviceName, IntPtr pEndpointFormat, IntPtr mixFormat);
        int GetProcessingPeriod(string pszDeviceName, int bDefault, IntPtr pmftDefaultPeriod, IntPtr pmftMinimumPeriod);
        int SetProcessingPeriod(string pszDeviceName, IntPtr pmftPeriod);
        int GetShareMode(string pszDeviceName, IntPtr pMode);
        int SetShareMode(string pszDeviceName, int mode);
        int GetPropertyValue(string pszDeviceName, int bFxStore, IntPtr key, IntPtr pv);
        int SetPropertyValue(string pszDeviceName, int bFxStore, IntPtr key, IntPtr pv);
        int SetDefaultEndpoint(string pszDeviceName, int role);
        int SetEndpointVisibility(string pszDeviceName, int bVisible);
    }

    public static void SetDefaultRender(string id)
    {
        var cfg = (IPolicyConfig)(new CPolicyConfigClient());
        for (int role = 0; role <= 2; role++)
            cfg.SetDefaultEndpoint(id, role);
    }
}
'@

Add-Type -TypeDefinition $code -Language CSharp

$flowValue = if ($Flow -eq "Render") { 0 } else { 1 }
$ids = [AudioProbeUtil]::ListIds($flowValue)
if ($SetDefaultDevice) {
    $target = $ids | Where-Object { $_ -like "*$Id*" } | Select-Object -First 1
    if ($target) {
        [AudioProbeUtil]::SetDefaultRender($target)
        "default $Flow device set to: $target"
    }
}
$result = @()
foreach ($endpointId in $ids) {
    $guid = $null
    if ($endpointId -match '\{([0-9a-fA-F-]{36})\}$') { $guid = $Matches[1] }
    $friendly = $null
    if ($guid) {
        $flowKey = if ($Flow -eq "Render") { "Render" } else { "Capture" }
        $key = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\$flowKey\$guid\Properties"
        $props = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        $friendly = $props.'{f19f064d-082c-4e27-bc73-6882a1bb8e6c},4'
        if (-not $friendly) { $friendly = $props.'{a45c254e-df1c-4efd-8020-67d146a850e0},2' }
    }
    if (-not $Id -or $endpointId -like "*$Id*") {
        if ($SetScalar -ge 0.0) {
            [AudioProbeUtil]::SetScalar($endpointId, $SetScalar)
        }
        if ($SetMute -eq "on") {
            [AudioProbeUtil]::SetMute($endpointId, $true)
        } elseif ($SetMute -eq "off") {
            [AudioProbeUtil]::SetMute($endpointId, $false)
        }
    }
    $scalar = -1.0
    $mute = $false
    try { $scalar = [AudioProbeUtil]::GetScalar($endpointId) } catch { }
    try { $mute = [AudioProbeUtil]::GetMute($endpointId) } catch { }
    $result += [pscustomobject]@{
        Friendly = $friendly
        Scalar   = $scalar
        Mute     = $mute
        Id       = $endpointId
    }
}
$result | ForEach-Object {
    $volTxt = if ($_.Scalar -lt 0) { "  ERR" } else { "{0,5:P0}" -f $_.Scalar }
    "{0,-36} vol={1} mute={2} id={3}" -f $_.Friendly, $volTxt, $_.Mute, $_.Id
}
