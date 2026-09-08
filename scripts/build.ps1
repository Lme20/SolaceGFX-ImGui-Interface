[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Release",

    [string] $BuildDirectory = "build",

    [switch] $Rebuild,
    [switch] $Run,
    [switch] $StopRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repositoryRoot $BuildDirectory
$applicationPath = Join-Path $buildPath "bin\$Configuration\Solace.exe"

function Find-CMake {
    $command = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    # Visual Studio bundles CMake; locate it through vswhere as a fallback.
    $installerRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $installerRoot "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installation) {
            $candidate = Join-Path $installation.Trim() `
                "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }

    throw "cmake.exe was not found. Install CMake 3.24+ or the Visual Studio 'C++ CMake tools' component."
}

if ($StopRunning -and (Test-Path -LiteralPath $applicationPath)) {
    $resolvedApplication = [IO.Path]::GetFullPath($applicationPath)
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object {
            $_.ExecutablePath -and
            [IO.Path]::GetFullPath($_.ExecutablePath) -ieq $resolvedApplication
        } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
}

$cmake = Find-CMake

if ($Rebuild -and (Test-Path -LiteralPath $buildPath)) {
    Write-Host "Removing $buildPath..."
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

Write-Host "Configuring (Visual Studio 2022, x64)..."
& $cmake -S $repositoryRoot -B $buildPath -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

Write-Host "Building Solace ($Configuration)..."
& $cmake --build $buildPath --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE." }

if ($Run) {
    if (-not (Test-Path -LiteralPath $applicationPath)) {
        throw "The application executable was not produced: $applicationPath"
    }
    Start-Process -FilePath $applicationPath -WorkingDirectory $repositoryRoot
}

Write-Host "Build completed successfully."
