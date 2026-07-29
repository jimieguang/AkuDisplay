param(
    [Parameter(Mandatory=$true)][string]$Raw,
    [Parameter(Mandatory=$true)][string]$Png,
    [int]$W = 162,
    [int]$H = 132
)
# Convert a raw RGB565 little-endian framebuffer dump to PNG (4x upscale for viewing).
Add-Type -AssemblyName System.Drawing
$bytes = [System.IO.File]::ReadAllBytes($Raw)
$bmp = New-Object System.Drawing.Bitmap($W, $H)
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x++) {
        $i = ($y * $W + $x) * 2
        $v = [int]$bytes[$i] -bor ([int]$bytes[$i+1] -shl 8)
        $r = (($v -shr 11) -band 0x1F) * 8
        $g = (($v -shr 5) -band 0x3F) * 4
        $b = ($v -band 0x1F) * 8
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($r, $g, $b))
    }
}
$scale = 4
$bw = $W * $scale
$bh = $H * $scale
$big = New-Object System.Drawing.Bitmap -ArgumentList $bw, $bh
$g2 = [System.Drawing.Graphics]::FromImage($big)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$g2.DrawImage($bmp, 0, 0, $bw, $bh)
$big.Save($Png, [System.Drawing.Imaging.ImageFormat]::Png)
$g2.Dispose(); $big.Dispose(); $bmp.Dispose()
Write-Output "saved: $Png"
