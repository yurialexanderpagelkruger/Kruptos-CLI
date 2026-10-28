#Requires -Version 5.1
<#
    Instalador portable de Kruptos para Windows.

    Copia kruptos.exe a %LOCALAPPDATA%\Programs\Kruptos y, salvo -NoPath,
    agrega esa carpeta al PATH del usuario. No requiere administrador.

    Uso:
      .\install.ps1
      .\install.ps1 -NoPath
      .\install.ps1 -InstallDir "D:\Tools\Kruptos"
      .\install.ps1 -Uninstall
      .\install.ps1 -Source "C:\ruta\kruptos.exe"
#>
[CmdletBinding()]
param(
    [string]$InstallDir,
    [string]$Source,
    [switch]$NoPath,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'

if (-not $InstallDir) {
    $InstallDir = Join-Path $env:LOCALAPPDATA 'Programs\Kruptos'
}
$InstallDir = [IO.Path]::GetFullPath($InstallDir)

function Normalize-PathEntry([string]$Value) {
    if ($null -eq $Value) { return '' }
    return $Value.Trim().TrimEnd('\')
}

function Get-UserPath {
    $value = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($null -eq $value) { return '' }
    return $value
}

function Save-UserPath([string]$Value) {
    if ([string]::IsNullOrEmpty($Value)) {
        [Environment]::SetEnvironmentVariable('Path', $null, 'User')
    } else {
        [Environment]::SetEnvironmentVariable('Path', $Value, 'User')
    }
}

if ($Uninstall) {
    $normalizedTarget = Normalize-PathEntry $InstallDir
    $userPath = Get-UserPath
    $entries = @($userPath -split ';' | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and
        ((Normalize-PathEntry $_) -ine $normalizedTarget)
    })
    $newPath = $entries -join ';'
    if ($newPath -ne $userPath) {
        Save-UserPath $newPath
        Write-Host "PATH del usuario actualizado: se quito $InstallDir"
    }

    if (Test-Path -LiteralPath $InstallDir) {
        Remove-Item -LiteralPath $InstallDir -Recurse -Force
        Write-Host "Carpeta eliminada: $InstallDir"
    }
    Write-Host "Kruptos desinstalado."
    return
}

function Find-Source {
    if ($Source) {
        $item = Get-Item -LiteralPath $Source -ErrorAction SilentlyContinue
        if ($null -eq $item) { throw "No existe el archivo: $Source" }
        return $item.FullName
    }

    $candidates = @(
        (Join-Path $PSScriptRoot 'kruptos.exe'),
        (Join-Path $PSScriptRoot 'Kruptos.exe')
    )
    if ([Environment]::Is64BitOperatingSystem) {
        $candidates += (Join-Path $PSScriptRoot 'bin\x64\Release\Kruptos.exe')
    }
    $candidates += (Join-Path $PSScriptRoot 'bin\Win32\Release\Kruptos.exe')
    $candidates += (Join-Path $PSScriptRoot 'bin\x64\Debug\Kruptos.exe')

    foreach ($candidate in $candidates) {
        $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
        if ($null -ne $item) { return $item.FullName }
    }
    return $null
}

$sourcePath = Find-Source
if ($null -eq $sourcePath) {
    throw 'No se encontro Kruptos.exe. Compila con .\build.ps1 o indica -Source <ruta>.'
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$target = Join-Path $InstallDir 'kruptos.exe'
Copy-Item -LiteralPath $sourcePath -Destination $target -Force

Write-Host "Instalado: $target"

if (-not $NoPath) {
    $normalizedTarget = Normalize-PathEntry $InstallDir
    $userPath = Get-UserPath
    $entries = @($userPath -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

    $alreadyPresent = $false
    foreach ($entry in $entries) {
        if ((Normalize-PathEntry $entry) -ieq $normalizedTarget) {
            $alreadyPresent = $true
            break
        }
    }

    if (-not $alreadyPresent) {
        $entries += $InstallDir
        Save-UserPath ($entries -join ';')
        Write-Host "Anadido al PATH del usuario: $InstallDir"
    } else {
        Write-Host "El PATH del usuario ya contiene: $InstallDir"
    }

    Write-Host ''
    Write-Host 'Abre una nueva PowerShell y escribe:'
    Write-Host '  kruptos --help'
    Write-Host ''
    Write-Host 'Para usarlo en esta misma sesion, ejecuta:'
    Write-Host '  $env:Path = [Environment]::GetEnvironmentVariable(''Path'',''Machine'') + '';'' + [Environment]::GetEnvironmentVariable(''Path'',''User'')'
} else {
    Write-Host ''
    Write-Host 'PATH sin cambios. Ejecuta directamente:'
    Write-Host "  & `"$target`" --help"
}

Write-Host ''
Write-Host 'Kruptos instalado correctamente.'
