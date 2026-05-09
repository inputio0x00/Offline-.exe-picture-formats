# ImgConvert

An offline, batch image-format converter for Windows. Bidirectional
conversion between **HEIF / HEIC**, **JPG**, **PNG**, **WEBP**, and
**TIFF**, packaged as a single self-contained `.exe` (no installer, no
runtime dependencies).

- **Native C++ core** (`img_convert.dll`) — decode/encode performed by
  libheif + x265 (HEIF), libjpeg-turbo (JPG), libpng (PNG), libtiff
  (TIFF), and libwebp (WEBP).
- **Modern UI** — C# WPF on .NET 8 with [WPF-UI](https://wpfui.lepo.co/)
  Fluent design (Mica backdrop, dark theme).
- **Batch** — drag-and-drop or browse to add many files at once,
  parallelized across CPU cores.
- **Format-aware options** — quality slider for lossy formats; lossless
  toggle for WEBP / HEIF; PNG zlib level; TIFF compression (LZW, ZIP,
  PackBits, none); JPEG progressive output. Alpha is preserved when both
  source and target support it.

## Repository layout

```
native/        C++ DLL (img_convert.dll)
  include/     Public C ABI header
  src/         Codecs + dispatch
  CMakeLists.txt
  vcpkg.json   Manifest of native deps
app/           C# WPF frontend (.NET 8)
  ImgConvert.csproj
  MainWindow.xaml{,.cs}
  ViewModels/  MainViewModel
  Services/    NativeBridge (P/Invoke), ConversionService
  Models/      ConversionItem
docs/BUILD.md  Full build guide
build.ps1      One-shot build (native + managed + single-file publish)
```

## Quick start (build from source on Windows)

Prerequisites: Visual Studio 2022 (Desktop C++ workload), .NET 8 SDK,
git, CMake, PowerShell. Then:

```powershell
.\build.ps1
```

Output: `dist\ImgConvert.exe`. See [`docs/BUILD.md`](docs/BUILD.md) for
details, license notes (x265 is GPL!), and the optional app-icon hook.

## Status

This repository contains source only — there is no pre-built binary.
The first `build.ps1` run takes 15–40 minutes because vcpkg compiles
every native dependency from source against the static MSVC runtime.
Subsequent builds are fast.
