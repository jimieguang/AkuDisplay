param([string]$B64File, [string]$Out)
$b = (Get-Content $B64File -Raw).Trim()
[IO.File]::WriteAllBytes((Join-Path (Get-Location) $Out), [Convert]::FromBase64String($b))
Write-Output ("decoded: {0} -> {1} bytes" -f $Out, (Get-Item $Out).Length)
