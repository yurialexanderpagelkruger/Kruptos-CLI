#Requires -Version 5.1
<#
    Compila Kruptos con MSBuild (Visual Studio 2026).
    Uso:  .\build.ps1 [-Platform x64|Win32] [-Configuration Release|Debug]
#>
[CmdletBinding()]
param(
    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'No se encontró vswhere.exe. Instala Visual Studio 2026 con la carga de trabajo de C++.'
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) {
    throw 'No se encontró MSBuild. Instala Visual Studio 2026 con la carga de trabajo de C++.'
}

& $msbuild (Join-Path $root 'Kruptos.vcxproj') `
    "/p:Configuration=$Configuration" "/p:Platform=$Platform" `
    /m /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    throw 'La compilación ha fallado.'
}

Write-Host ''
Write-Host "Listo: $root\bin\$Platform\$Configuration\Kruptos.exe" -ForegroundColor Green
