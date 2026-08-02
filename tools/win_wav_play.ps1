param(
    [string]$Wav,
    [string]$Id,
    [int]$Loops = 1
)

# WaveOut (winmm) player: plays a 16-bit PCM WAV to a specific waveOut
# device matched by name substring. Used to verify the UAC gadget forward
# path without relying on the system default device.

$code = @'
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class WaveOutPlayer
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct WAVEOUTCAPS
    {
        public ushort wMid; public ushort wPid; public uint vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname;
        public uint dwFormats; public ushort wChannels; public ushort wReserved1;
        public uint dwSupport;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEFORMATEX
    {
        public ushort wFormatTag; public ushort nChannels; public uint nSamplesPerSec;
        public uint nAvgBytesPerSec; public ushort nBlockAlign; public ushort wBitsPerSample;
        public ushort cbSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEHDR
    {
        public IntPtr lpData;
        public uint dwBufferLength;
        public uint dwBytesRecorded;
        public IntPtr dwUser;
        public uint dwFlags;
        public uint dwLoops;
        public IntPtr lpNext;
        public uint reserved;
    }

    [DllImport("winmm.dll")] public static extern uint waveOutGetNumDevs();
    [DllImport("winmm.dll")] public static extern int waveOutGetDevCaps(uint uDeviceID, ref WAVEOUTCAPS caps, uint cbCaps);
    [DllImport("winmm.dll")] public static extern int waveOutOpen(out IntPtr hwo, uint uDeviceID, ref WAVEFORMATEX pwfx, IntPtr dwCallback, IntPtr dwInstance, uint fdwOpen);
    [DllImport("winmm.dll")] public static extern int waveOutPrepareHeader(IntPtr hwo, ref WAVEHDR pwh, uint cbwh);
    [DllImport("winmm.dll")] public static extern int waveOutWrite(IntPtr hwo, ref WAVEHDR pwh, uint cbwh);
    [DllImport("winmm.dll")] public static extern int waveOutUnprepareHeader(IntPtr hwo, ref WAVEHDR pwh, uint cbwh);
    [DllImport("winmm.dll")] public static extern int waveOutClose(IntPtr hwo);

    private const uint WHDR_DONE = 0x1;
    private const uint WHDR_PREPARED = 0x2;
    private const uint WHDR_INQUEUE = 0x4;

    public static int FindByName(string sub)
    {
        uint n = waveOutGetNumDevs();
        for (uint i = 0; i < n; i++)
        {
            var c = new WAVEOUTCAPS();
            if (waveOutGetDevCaps(i, ref c, (uint)Marshal.SizeOf(typeof(WAVEOUTCAPS))) == 0)
                if (c.szPname.IndexOf(sub, StringComparison.OrdinalIgnoreCase) >= 0)
                    return (int)i;
        }
        return -1;
    }

    public static int Play(string nameSub, string wavPath, int loops)
    {
        int dev = FindByName(nameSub);
        if (dev < 0) return -100;

        byte[] data = File.ReadAllBytes(wavPath);
        if (data.Length < 44 || data[0] != (byte)'R' || data[8] != (byte)'W')
            return -10;
        ushort ch = BitConverter.ToUInt16(data, 22);
        uint rate = BitConverter.ToUInt32(data, 24);
        ushort bits = BitConverter.ToUInt16(data, 34);
        int dataStart = 44;
        for (int i = 12; i < data.Length - 8; i++)
            if (data[i] == (byte)'d' && data[i + 1] == (byte)'a' &&
                data[i + 2] == (byte)'t' && data[i + 3] == (byte)'a')
            { dataStart = i + 8; break; }
        int frameBytes = ch * bits / 8;
        if (frameBytes == 0) return -11;
        int frames = (data.Length - dataStart) / frameBytes;

        var fmt = new WAVEFORMATEX();
        fmt.wFormatTag = 1;
        fmt.nChannels = ch;
        fmt.nSamplesPerSec = rate;
        fmt.wBitsPerSample = bits;
        fmt.nBlockAlign = (ushort)frameBytes;
        fmt.nAvgBytesPerSec = rate * (uint)frameBytes;
        fmt.cbSize = 0;

        IntPtr hwo;
        int rc = waveOutOpen(out hwo, (uint)dev, ref fmt, IntPtr.Zero, IntPtr.Zero, 0);
        if (rc != 0) return rc;

        for (int loop = 0; loop < loops; loop++)
        {
            IntPtr buf = Marshal.AllocHGlobal(frames * frameBytes);
            Marshal.Copy(data, dataStart, buf, frames * frameBytes);
            var hdr = new WAVEHDR();
            hdr.lpData = buf;
            hdr.dwBufferLength = (uint)(frames * frameBytes);
            rc = waveOutPrepareHeader(hwo, ref hdr, (uint)Marshal.SizeOf(typeof(WAVEHDR)));
            if (rc != 0) { Marshal.FreeHGlobal(buf); return rc; }
            rc = waveOutWrite(hwo, ref hdr, (uint)Marshal.SizeOf(typeof(WAVEHDR)));
            if (rc != 0) { waveOutUnprepareHeader(hwo, ref hdr, (uint)Marshal.SizeOf(typeof(WAVEHDR))); Marshal.FreeHGlobal(buf); return rc; }
            int waited = 0;
            while ((hdr.dwFlags & WHDR_DONE) == 0 && waited < 20000)
            {
                System.Threading.Thread.Sleep(10);
                waited += 10;
            }
            waveOutUnprepareHeader(hwo, ref hdr, (uint)Marshal.SizeOf(typeof(WAVEHDR)));
            Marshal.FreeHGlobal(buf);
        }
        waveOutClose(hwo);
        return 0;
    }
}
'@

Add-Type -TypeDefinition $code -Language CSharp

if (-not $Wav) { "usage: win_wav_play.ps1 -Wav <file> -Id <name-substr> [-Loops n]"; exit 1 }
"playing '$Wav' (x$Loops) to waveOut device matching '$Id'"
$rc = [WaveOutPlayer]::Play($Id, $Wav, $Loops)
"done rc=$rc"
exit $rc
