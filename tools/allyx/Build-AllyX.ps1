# SPDX-License-Identifier: GPL-3.0-or-later
#requires -Version 5.1
<#
.SYNOPSIS
Builds the patched Lichii source with native Windows motion input.
.DESCRIPTION
Uses clang-cl 22 or newer with the Visual C++ SDK environment, Ninja, and an
installed MSVC Qt 6 kit. Dependencies are checked first; no tools are installed.
CMake downloads the dependencies declared by the repository during configure.
.EXAMPLE
.\tools\allyx\Build-AllyX.ps1 -QtRoot 'C:\Qt\6.8.3\msvc2022_64'
.EXAMPLE
.\tools\allyx\Build-AllyX.ps1 -CheckOnly
#>
[CmdletBinding()]
param(
    [string]$SourceRoot = '',
    [string]$QtRoot = '',
    [string]$BuildDir = '',
    [ValidateRange(1, 16)]
    [int]$Jobs = 2,
    [switch]$GenericCpu,
    [switch]$NoPgo,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitProcess -or
    $env:PROCESSOR_ARCHITECTURE -ne 'AMD64') {
    throw 'Run this Ally X build script in 64-bit PowerShell on x64 Windows.'
}

if (-not $SourceRoot) {
    $SourceRoot = Join-Path $PSScriptRoot '..\..'
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
foreach ($relative in @('CMakeLists.txt', 'src\input_common\drivers\windows_motion.cpp',
                         'tools\windows\load-msvc-env.ps1')) {
    if (-not (Test-Path -LiteralPath (Join-Path $SourceRoot $relative) -PathType Leaf)) {
        throw "This is not the patched source folder: $SourceRoot (missing $relative). Extract the complete source ZIP first."
    }
}
if (-not $BuildDir) {
    $BuildDir = Join-Path $SourceRoot 'build\allyx-z1e-clangcl'
}
$BuildDir = [IO.Path]::GetFullPath($BuildDir)

function Add-ProcessPath([string]$Directory) {
    if ($Directory -and (Test-Path -LiteralPath $Directory -PathType Container)) {
        $env:PATH = "$Directory;$env:PATH"
    }
}

function Find-QtRoot([string]$Candidate) {
    if (-not $Candidate -or -not (Test-Path -LiteralPath $Candidate -PathType Container)) {
        return $null
    }
    $candidatePath = (Resolve-Path -LiteralPath $Candidate).Path
    if (Test-Path -LiteralPath (Join-Path $candidatePath 'Qt6Config.cmake')) {
        $candidatePath = [IO.Path]::GetFullPath((Join-Path $candidatePath '..\..\..'))
    }
    if ((Test-Path -LiteralPath (Join-Path $candidatePath 'bin\windeployqt.exe')) -and
        (Test-Path -LiteralPath (Join-Path $candidatePath 'lib\cmake\Qt6\Qt6Config.cmake')) -and
        (Test-Path -LiteralPath (Join-Path $candidatePath 'lib\cmake\Qt6Charts\Qt6ChartsConfig.cmake'))) {
        return $candidatePath
    }
    return $null
}

$issues = [Collections.Generic.List[string]]::new()
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = $null
if (Test-Path -LiteralPath $vswhere) {
    $visualStudioRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $visualStudioRoot) {
    $issues.Add('Visual Studio 2022 / Build Tools: install Desktop development with C++, MSVC x64/x86 tools, and Windows SDK 10.0.19041 or newer.')
} else {
    $vcvars = Join-Path $visualStudioRoot 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path -LiteralPath $vcvars)) {
        $issues.Add("Visual C++ environment is incomplete: $vcvars was not found.")
    } else {
        & (Join-Path $SourceRoot 'tools\windows\load-msvc-env.ps1')
        Add-ProcessPath (Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin')
        Add-ProcessPath (Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja')
    }
}
Add-ProcessPath (Join-Path $env:ProgramFiles 'CMake\bin')
Add-ProcessPath (Join-Path $env:ProgramFiles 'Git\cmd')
if ($env:VULKAN_SDK) {
    Add-ProcessPath (Join-Path $env:VULKAN_SDK 'Bin')
}

foreach ($tool in @('link.exe', 'cmake.exe', 'ctest.exe', 'ninja.exe', 'git.exe', 'glslangValidator.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        switch ($tool) {
            'link.exe' { $issues.Add('The MSVC x64 linker is unavailable. Repair the Visual Studio C++ workload.') }
            'cmake.exe' { $issues.Add('CMake 3.31 or newer must be installed and available on PATH.') }
            'ctest.exe' { $issues.Add('CTest (included with CMake) must be available on PATH.') }
            'ninja.exe' { $issues.Add('Ninja must be on PATH (the Visual Studio C++ CMake tools include it).') }
            'git.exe' { $issues.Add('Git for Windows must be installed and available on PATH.') }
            'glslangValidator.exe' { $issues.Add('Install the Vulkan SDK, then open a new PowerShell window so its Bin directory is on PATH.') }
        }
    }
}
if (Get-Command cmake.exe -ErrorAction SilentlyContinue) {
    $cmakeVersionLine = & cmake.exe --version | Select-Object -First 1
    $cmakeVersionValid = $false
    if ($cmakeVersionLine -match '([0-9]+\.[0-9]+\.[0-9]+)') {
        $cmakeVersionValid = [version]$matches[1] -ge [version]'3.31.0'
    }
    if (-not $cmakeVersionValid) {
        $issues.Add("CMake 3.31 or newer is required; found: $cmakeVersionLine")
    }
}

$clangCandidates = [Collections.Generic.List[string]]::new()
$clangOnPath = Get-Command clang-cl.exe -ErrorAction SilentlyContinue
if ($env:LLVM_ROOT) { $clangCandidates.Add((Join-Path $env:LLVM_ROOT 'bin\clang-cl.exe')) }
if ($clangOnPath) { $clangCandidates.Add($clangOnPath.Source) }
$clangCandidates.Add((Join-Path $env:ProgramFiles 'LLVM\bin\clang-cl.exe'))
if ($visualStudioRoot) {
    $clangCandidates.Add((Join-Path $visualStudioRoot 'VC\Tools\Llvm\x64\bin\clang-cl.exe'))
    $clangCandidates.Add((Join-Path $visualStudioRoot 'VC\Tools\Llvm\bin\clang-cl.exe'))
}
$clangExecutable = $null
$clangVersion = $null
foreach ($candidate in ($clangCandidates | Select-Object -Unique)) {
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { continue }
    try {
        $clangVersionText = (& $candidate --version 2>$null) -join "`n"
        if ($LASTEXITCODE -eq 0 -and $clangVersionText -match 'clang version ([0-9]+\.[0-9]+\.[0-9]+)') {
            $candidateVersion = [version]$matches[1]
            if ($candidateVersion -ge [version]'22.0.0' -and
                $clangVersionText -match 'Target: x86_64-pc-windows-msvc') {
                $clangExecutable = (Resolve-Path -LiteralPath $candidate).Path
                $clangVersion = $candidateVersion
                break
            }
        }
    } catch {
        # Try another installed LLVM distribution.
    }
}
if (-not $clangExecutable) {
    $issues.Add('Install LLVM 22 or newer for Windows x64 with clang-cl.exe (the repository documents LLVM 22). Checked PATH, Program Files\LLVM, and Visual Studio Clang tools; this build does not fall back to cl.exe.')
} else {
    $llvmBin = Split-Path -Parent $clangExecutable
    Add-ProcessPath $llvmBin
    foreach ($tool in @('lld-link.exe', 'llvm-lib.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $llvmBin $tool))) {
            $issues.Add("LLVM is incomplete: $tool is missing from $llvmBin.")
        }
    }
}

$winrtHeaderFound = $false
$sensorSdkVersion = $null
foreach ($includeDirectory in ($env:INCLUDE -split ';')) {
    if ($includeDirectory -and (Test-Path -LiteralPath (Join-Path $includeDirectory 'winrt\Windows.Devices.Sensors.h'))) {
        $winrtHeaderFound = $true
        if ($includeDirectory -match '[\\/]Include[\\/](10\.0\.[0-9]+\.[0-9]+)[\\/]cppwinrt[\\/]?$') {
            $sensorSdkVersion = [version]$matches[1]
        }
        break
    }
}
if (-not $winrtHeaderFound) {
    $issues.Add('Windows SDK C++/WinRT sensor headers are unavailable. Install Windows SDK 10.0.19041 or newer in Visual Studio Installer.')
} elseif ($null -ne $sensorSdkVersion -and $sensorSdkVersion -lt [version]'10.0.19041.0') {
    $issues.Add("Windows SDK $sensorSdkVersion is too old. The motion backend requires Windows SDK 10.0.19041 or newer.")
}

$pythonExecutable = $null
foreach ($pythonName in @('python3.exe', 'python.exe', 'py.exe')) {
    $pythonCommand = Get-Command $pythonName -ErrorAction SilentlyContinue
    if (-not $pythonCommand -or $pythonCommand.Source -like '*\Microsoft\WindowsApps\*') {
        continue
    }
    $pythonArguments = @('-c', 'import sys; assert sys.version_info >= (3, 10); print(sys.executable)')
    if ($pythonName -eq 'py.exe') {
        $pythonArguments = @('-3') + $pythonArguments
    }
    try {
        $pythonResult = @(& $pythonCommand.Source @pythonArguments 2>$null)
        if ($LASTEXITCODE -eq 0 -and $pythonResult.Count -gt 0 -and
            (Test-Path -LiteralPath $pythonResult[-1] -PathType Leaf)) {
            $pythonExecutable = $pythonResult[-1]
            break
        }
    } catch {
        # Try the next Python launcher.
    }
}
if (-not $pythonExecutable) {
    $issues.Add('Install Python 3.10 or newer, including the Python launcher or its executable on PATH.')
}

if ($QtRoot) {
    $resolvedQtRoot = Find-QtRoot $QtRoot
} else {
    $qtCandidates = [Collections.Generic.List[string]]::new()
    foreach ($candidate in @($env:QT_ROOT_DIR, $env:QTDIR, $env:Qt6_DIR, 'C:\Qt\6.8.3\msvc2022_64')) {
        if ($candidate) { $qtCandidates.Add($candidate) }
    }
    if (Test-Path -LiteralPath 'C:\Qt') {
        Get-ChildItem -LiteralPath 'C:\Qt' -Directory | Sort-Object Name -Descending | ForEach-Object {
            $qtCandidates.Add((Join-Path $_.FullName 'msvc2022_64'))
        }
    }
    $resolvedQtRoot = $null
    foreach ($candidate in $qtCandidates) {
        $resolvedQtRoot = Find-QtRoot $candidate
        if ($resolvedQtRoot) { break }
    }
}
if (-not $resolvedQtRoot) {
    $issues.Add('Install a 64-bit MSVC Qt 6 kit with Qt Charts (release CI uses Qt 6.8.3 msvc2022_64). Pass -QtRoot C:\Qt\6.8.3\msvc2022_64 if needed.')
} elseif ($resolvedQtRoot -match 'mingw|arm64') {
    $issues.Add("The Qt kit must be MSVC x64, not MinGW/ARM64: $resolvedQtRoot")
}

$cachePath = Join-Path $BuildDir 'CMakeCache.txt'
if ($clangExecutable -and (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
    foreach ($cacheLine in (Get-Content -LiteralPath $cachePath)) {
        if ($cacheLine -match '^CMAKE_(?:C|CXX)_COMPILER:[^=]+=(.+)$') {
            $cachedCompiler = $matches[1]
            $compilerConflicts = [IO.Path]::GetFileName($cachedCompiler) -ine 'clang-cl.exe'
            if (-not $compilerConflicts -and [IO.Path]::IsPathRooted($cachedCompiler)) {
                $compilerConflicts = -not [string]::Equals([IO.Path]::GetFullPath($cachedCompiler),
                    $clangExecutable, [StringComparison]::OrdinalIgnoreCase)
            }
            if ($compilerConflicts) {
                $issues.Add("The build directory caches a different compiler: $cachedCompiler. Pass -BuildDir with a new directory; the existing build has not been removed.")
                break
            }
        }
        if ($cacheLine -match '^CMAKE_GENERATOR:INTERNAL=(.+)$' -and $matches[1] -ne 'Ninja') {
            $issues.Add('The build directory uses a different CMake generator. Pass -BuildDir with a new directory for this Ninja build.')
            break
        }
    }
}

if ($issues.Count -gt 0) {
    Write-Host "`nBuild prerequisites need attention:" -ForegroundColor Yellow
    foreach ($issue in $issues) { Write-Host " - $issue" }
    Write-Host "`nThe complete prerequisite list is in docs\Deps.md. No tools were installed."
    throw 'Prerequisite check failed. Fix the listed items and run this script again.'
}

Add-ProcessPath (Join-Path $resolvedQtRoot 'bin')
Write-Host "Source: $SourceRoot"
Write-Host "Clang:  $clangExecutable ($clangVersion)"
Write-Host "Qt:     $resolvedQtRoot"
Write-Host "Build:  $BuildDir"
Write-Host "Jobs:   $Jobs"
if ($CheckOnly) {
    Write-Host 'Prerequisite check passed. Remove -CheckOnly to configure and build.'
    return
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
$logPath = Join-Path $BuildDir ('allyx-build-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
$transcriptStarted = $false
try {
    Start-Transcript -Path $logPath | Out-Null
    $transcriptStarted = $true
    # Match the release's ThinLTO toolchain; tune generated host code for Zen 4.
    # GenericCpu permits comparing the same patch without CPU-specific compilation.
    $cpuFlags = if ($GenericCpu) { '/clang:-march=x86-64-v3 /clang:-mtune=generic' } else { '' }
    $commonFlags = "-DWIN32 -D_WINDOWS -DNOMINMAX -flto=thin $cpuFlags"
    if (-not $NoPgo) {
        $profile = Join-Path $SourceRoot 'lichii-windows.profdata'
        if (-not (Test-Path -LiteralPath $profile -PathType Leaf)) {
            throw 'The upstream Windows PGO profile is missing. Restore it or use -NoPgo.'
        }
        $profile = $profile.Replace('\', '/')
        $commonFlags += " -fprofile-use=`"$profile`" -Wno-error=profile-instr-out-of-date -Wno-error=profile-instr-unprofiled -Wno-error=backend-plugin"
    }
    $configureArguments = @(
        '-S', $SourceRoot, '-B', $BuildDir, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TRY_COMPILE_CONFIGURATION=Release',
        ('-DLICHII_ALLYX_Z1E=' + (-not $GenericCpu).ToString()),
        "-DCMAKE_C_COMPILER=$clangExecutable", "-DCMAKE_CXX_COMPILER=$clangExecutable",
        "-DCMAKE_LINKER=$llvmBin\lld-link.exe", "-DCMAKE_AR=$llvmBin\llvm-lib.exe",
        "-DCMAKE_EXE_LINKER_FLAGS=/threads:$Jobs",
        "-DCMAKE_C_FLAGS=$commonFlags", "-DCMAKE_CXX_FLAGS=-EHsc $commonFlags",
        '-DBUILD_TESTING=OFF', '-DYUZU_TESTS=OFF', '-DYUZU_CMD=OFF',
        '-DYUZU_ROOM=OFF', '-DYUZU_ROOM_STANDALONE=OFF',
        '-DYUZU_USE_BUNDLED_QT=OFF', '-DYUZU_USE_BUNDLED_SDL3=ON',
        '-DENABLE_QT=ON', '-DENABLE_WINDOWS_MOTION=ON',
        '-DENABLE_UPDATE_CHECKER=OFF', '-DUSE_DISCORD_PRESENCE=OFF',
        "-DQt6_DIR=$resolvedQtRoot\lib\cmake\Qt6",
        "-DPython3_EXECUTABLE=$pythonExecutable"
    )
    # Run the coordinate/timing regression checks even in this Release build.
    $testBuild = Join-Path $BuildDir 'motion-checks'
    & cmake.exe -S (Join-Path $SourceRoot 'tools\allyx') -B $testBuild -G Ninja `
        '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$clangExecutable"
    if ($LASTEXITCODE -ne 0) { throw "Motion test configure failed. See $logPath" }
    & cmake.exe --build $testBuild
    if ($LASTEXITCODE -ne 0) { throw "Motion test build failed. See $logPath" }
    & ctest.exe --test-dir $testBuild --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw "Motion checks failed. See $logPath" }

    & cmake.exe @configureArguments
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed. See $logPath" }
    if (-not (Select-String -LiteralPath $cachePath -Pattern '^ENABLE_WINDOWS_MOTION:BOOL=ON$' -Quiet)) {
        throw 'CMake did not enable native Windows motion. Use the patched source and a supported Windows SDK.'
    }

    & cmake.exe --build $BuildDir --target yuzu --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build failed. See $logPath. If memory was exhausted, retry with -Jobs 2." }

    $binaryDirectory = Join-Path $BuildDir 'bin'
    $emulator = Join-Path $binaryDirectory 'lichii.exe'
    if (-not (Test-Path -LiteralPath $emulator -PathType Leaf)) {
        throw "The expected executable was not produced: $emulator"
    }
    $deployTool = Join-Path $resolvedQtRoot 'bin\windeployqt.exe'
    & $deployTool --release --dir $binaryDirectory --libdir $binaryDirectory --plugindir (Join-Path $binaryDirectory 'plugins') --no-translations $emulator
    if ($LASTEXITCODE -ne 0) { throw "Qt runtime deployment failed. See $logPath" }
    if (-not (Test-Path -LiteralPath (Join-Path $binaryDirectory 'plugins\platforms\qwindows.dll'))) {
        throw 'Qt Windows platform plugin was not deployed successfully.'
    }
    Write-Host "`nBuild complete: $emulator" -ForegroundColor Green
    Write-Host 'Keep the entire bin folder together, including its DLLs, plugins, and data files.'
    Write-Host 'This executable is built from the patched source; sensor operation still needs testing on your Ally X.'
    Write-Host "Build log: $logPath"
} finally {
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
}
