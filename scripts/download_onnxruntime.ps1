param(
    [string]$Version = "1.23.2",
    [string]$OutputDir = "deps"
)

$ErrorActionPreference = "Stop"

$name = "onnxruntime-win-x64-$Version"
$url = "https://github.com/microsoft/onnxruntime/releases/download/v$Version/$name.zip"
$outDir = [System.IO.Path]::GetFullPath($OutputDir)
$zipPath = Join-Path $outDir "$name.zip"
$targetDir = Join-Path $outDir $name

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Invoke-WebRequest -Uri $url -OutFile $zipPath

if (Test-Path -LiteralPath $targetDir) {
    Remove-Item -LiteralPath $targetDir -Recurse -Force
}

Expand-Archive -LiteralPath $zipPath -DestinationPath $outDir -Force

Write-Host "Downloaded $targetDir"
