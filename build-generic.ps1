<#
.SYNOPSIS
Build Luma Clock using a 64-bit MinGW-w64 GCC toolchain.
.EXAMPLE
.\build-generic.ps1
.EXAMPLE
.\build-generic.ps1 -Compiler 'C:\Toolchains\mingw64\bin\g++.exe' -Debug
.EXAMPLE
.\build-generic.ps1 -Compiler g++.exe -ResourceCompiler windres.exe
#>
param(
    [string]$Compiler = 'g++.exe',
    [switch]$Debug,
    [string]$ResourceCompiler = ''
)

$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot

# Accept either an executable path or an executable name available on PATH.
function Resolve-BuildTool {
    param([string]$Name, [string]$Description)
    $tool = Get-Command -Name $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $tool) {
        throw "$Description not found: $Name. Add its directory to PATH or specify its executable path."
    }
    return $tool.Source
}

$Compiler = Resolve-BuildTool -Name $Compiler -Description 'MinGW-w64 g++'
$compilerDirectory = Split-Path -Parent $Compiler

# Prefer the resource compiler from the same toolchain. Prefixed tool names
# such as x86_64-w64-mingw32-g++ / x86_64-w64-mingw32-windres also work.
if (-not $ResourceCompiler) {
    $compilerName = Split-Path -Leaf $Compiler
    $resourceName = $compilerName -replace 'g\+\+(\.exe)?$', 'windres.exe'
    $resourceCandidates = @(
        (Join-Path $compilerDirectory $resourceName),
        (Join-Path $compilerDirectory 'windres.exe')
    ) | Select-Object -Unique
    foreach ($candidate in $resourceCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $ResourceCompiler = $candidate
            break
        }
    }
    if (-not $ResourceCompiler) { $ResourceCompiler = 'windres.exe' }
}
$ResourceCompiler = Resolve-BuildTool -Name $ResourceCompiler -Description 'MinGW-w64 windres'

$buildDirectory = Join-Path $projectDirectory 'build'
$appDirectory = Join-Path $projectDirectory 'app'
$originalPath = $env:PATH
try {
    # This process-local change also helps each tool find its companion DLLs.
    $toolDirectories = @($compilerDirectory, (Split-Path -Parent $ResourceCompiler)) |
        Select-Object -Unique
    $env:PATH = ($toolDirectories -join ';') + ';' + $originalPath

    $target = (& $Compiler '-dumpmachine' | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Could not determine the compiler target.' }
    if ($target -ne 'x86_64-w64-mingw32') {
        throw "This project requires x64 MinGW-w64 GCC (x86_64-w64-mingw32). Selected target: $target"
    }

    New-Item -ItemType Directory -Path $buildDirectory,$appDirectory -Force | Out-Null

    # Compile icon, manifest and version information into a COFF object file.
    Push-Location (Join-Path $projectDirectory 'resources')
    try {
        & $ResourceCompiler '-i' 'app.rc' '-o' (Join-Path $buildDirectory 'resources.o') '-O' 'coff' '--codepage=65001'
        if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
    }
    finally { Pop-Location }

    $output = Join-Path $appDirectory 'LumaClock.exe'
    $compilerArguments = @(
        '-std=c++17','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00',
        '-Wall','-Wextra','-Wpedantic','-Wno-missing-field-initializers',
        '-municode','-mwindows','-static','-ffunction-sections','-fdata-sections'
    )
    if ($Debug) { $compilerArguments += @('-Og','-g') }
    else { $compilerArguments += @('-Os','-s') }

    # Compile C++ sources and link them with the resources and Windows libraries.
    $compilerArguments += @(
        (Join-Path $projectDirectory 'src\main.cpp'),
        (Join-Path $projectDirectory 'src\clock.cpp'),
        (Join-Path $projectDirectory 'src\settings.cpp'),
        (Join-Path $buildDirectory 'resources.o'),'-o',$output,'-Wl,--gc-sections',
        '-lgdiplus','-ldwmapi','-lcomctl32','-lshell32','-ladvapi32','-lgdi32','-luser32','-lole32','-luuid'
    )
    & $Compiler @compilerArguments
    if ($LASTEXITCODE -ne 0) { throw 'C++ compilation or linking failed. Exit the running clock before rebuilding.' }

    $file = Get-Item -LiteralPath $output
    Write-Output ('Build succeeded: {0} ({1:N1} KiB)' -f $file.FullName, ($file.Length / 1KB))
}
finally {
    # Restore PATH even if compilation fails.
    $env:PATH = $originalPath
}
