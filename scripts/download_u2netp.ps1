param(
    [string]$OutputDir = "models"
)

$ErrorActionPreference = "Stop"

$url = "https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx"
$outDir = [System.IO.Path]::GetFullPath($OutputDir)
$outFile = Join-Path $outDir "u2netp.onnx"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Invoke-WebRequest -Uri $url -OutFile $outFile

Write-Host "Downloaded $outFile"
