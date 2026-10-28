<#
    Crea una distribucion portable de Kruptos para Windows en dist\.
    El resultado incluye kruptos.exe (autocontenido), el instalador y un LEEME.

    Uso:
      .\publish.ps1
      .\publish.ps1 -Platform Win32
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

& (Join-Path $root 'build.ps1') -Platform $Platform

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$exe = Join-Path $root "bin\$Platform\Release\Kruptos.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "No se encontro el ejecutable: $exe"
}

Copy-Item -LiteralPath $exe -Destination (Join-Path $dist 'kruptos.exe') -Force
Copy-Item -LiteralPath (Join-Path $root 'install.ps1') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'install.cmd') -Destination $dist -Force
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination (Join-Path $dist 'LEEME.md') -Force

$shortReadme = @"
Kruptos - cifrado multicapa adaptativo
======================================

1. Ejecuta install.cmd
2. Abre una nueva PowerShell
3. Escribe: kruptos --help

El ejecutable es autocontenido: no necesita Visual C++ Redistributable,
.NET ni permisos de administrador.

Ejemplos:
  kruptos encrypt -m "Hola mundo"
  kruptos decrypt -i "KR1-..." -m "..."
  kruptos --lang en --help

Desinstalar:
  install.cmd -Uninstall
"@
Set-Content -LiteralPath (Join-Path $dist 'LEEME.txt') -Value $shortReadme -Encoding ASCII

$zip = Join-Path $root "Kruptos-windows-$Platform.zip"
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip -CompressionLevel Optimal

Write-Host ''
Write-Host "Distribucion: $dist"
Write-Host "Paquete ZIP: $zip"
