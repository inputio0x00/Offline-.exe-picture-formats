#requires -Version 5.1
<#
.SYNOPSIS
    One-shot build for ImgConvert: native C++ DLL + C# single-file .exe.

.DESCRIPTION
    Steps:
      1. Bootstraps vcpkg into ./vcpkg/ on first run (clone + bootstrap-vcpkg.bat).
      2. Configures and builds the native DLL (img_convert.dll) using the
         x64-windows-static triplet so all C++ dependencies (libheif, x265,
         libpng, libjpeg-turbo, libtiff, libwebp) are statically linked into
         a single DLL with no extra runtime files.
      3. Runs `dotnet publish` with PublishProfile=SingleFile to produce a
         self-contained single-file .exe under ./dist/.

.PARAMETER Configuration
    Build configuration. Default: Release.

.PARAMETER SkipNative
    Skip native build (useful when only the C# UI changed).

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Configuration Release
    .\build.ps1 -SkipNative
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipNative
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root      = Split-Path -Parent $MyInvocation.MyCommand.Path
$nativeDir = Join-Path $root 'native'
$appDir    = Join-Path $root 'app'
$buildDir  = Join-Path $nativeDir 'build'
$vcpkgDir  = Join-Path $root 'vcpkg'
$distDir   = Join-Path $root 'dist'

function Write-Step($msg) {
    Write-Host ""
    Write-Host "==> $msg" -ForegroundColor Cyan
}

# ----- Native build -----
if (-not $SkipNative) {
    if (-not (Test-Path $vcpkgDir)) {
        Write-Step "Bootstrapping vcpkg into $vcpkgDir"
        git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgDir
        & (Join-Path $vcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics
        if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed." }
    }

    Write-Step "Configuring native (CMake + vcpkg manifest, triplet=x64-windows-static)"
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

    $toolchain = Join-Path $vcpkgDir 'scripts/buildsystems/vcpkg.cmake'
    cmake `
        -S $nativeDir `
        -B $buildDir `
        -A x64 `
        -DCMAKE_BUILD_TYPE=$Configuration `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DVCPKG_TARGET_TRIPLET=x64-windows-static `
        -DVCPKG_HOST_TRIPLET=x64-windows-static
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    Write-Step "Building native ($Configuration)"
    cmake --build $buildDir --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw "Native build failed." }

    $dllSrc = Join-Path $buildDir "$Configuration/img_convert.dll"
    if (-not (Test-Path $dllSrc)) {
        # Some generators put the DLL directly in build/ instead of build/<Config>/.
        $alt = Join-Path $buildDir 'img_convert.dll'
        if (Test-Path $alt) { $dllSrc = $alt }
    }
    if (-not (Test-Path $dllSrc)) { throw "Could not find built img_convert.dll." }

    $dllDst = Join-Path $appDir 'runtimes/win-x64/native/img_convert.dll'
    New-Item -ItemType Directory -Force -Path (Split-Path $dllDst) | Out-Null
    Copy-Item -Force $dllSrc $dllDst
    Write-Host "Native DLL: $dllDst"
}

# ----- Managed build / publish -----
Write-Step "Publishing C# single-file .exe"
if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir | Out-Null }

dotnet publish `
    (Join-Path $appDir 'ImgConvert.csproj') `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -p:PublishSingleFile=true `
    -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:IncludeAllContentForSelfExtract=true `
    -p:EnableCompressionInSingleFile=true `
    -p:_IsPublishing=true `
    -o $distDir
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed." }

$exe = Join-Path $distDir 'ImgConvert.exe'
if (Test-Path $exe) {
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 1)
    Write-Step "Done. Single-file exe: $exe (${size} MB)"
} else {
    throw "Expected $exe but it was not produced."
}
