# Building ImgConvert

ImgConvert is built in two halves that get bundled into one self-contained
`.exe`:

1. **`native/`** — a C++ DLL (`img_convert.dll`) that does all decoding and
   encoding. Statically linked against libheif (with x265 for HEIF
   encoding), libde265, libpng, libjpeg-turbo, libtiff, and libwebp.
2. **`app/`** — a C# WPF (.NET 8) frontend with a Fluent-design UI
   (WPF-UI). It calls into the DLL via P/Invoke.

The repo's `build.ps1` orchestrates both and produces `dist/ImgConvert.exe`,
a single self-contained file (~50–80 MB).

## Prerequisites (Windows 10/11, x64)

- **Visual Studio 2022** with the *Desktop development with C++* workload,
  or **Build Tools for Visual Studio 2022** (same workload).
- **CMake** 3.21+ (bundled with VS 2022, or install standalone).
- **Git** (so `build.ps1` can clone vcpkg).
- **.NET 8 SDK**: <https://dotnet.microsoft.com/download/dotnet/8.0>.
- **PowerShell** 5.1+ (ships with Windows) or PowerShell 7.

You do *not* need to install vcpkg yourself — the build script bootstraps a
local copy in `./vcpkg/` on first run.

## Build

From a *Developer PowerShell for VS 2022* (or any shell that has `cmake`,
`git`, and `dotnet` on PATH):

```powershell
.\build.ps1
```

First run takes 15–40 minutes — vcpkg compiles every C++ dependency from
source against the static MSVC runtime. Subsequent builds are seconds for
the C# side and minutes for native edits.

Useful flags:

```powershell
.\build.ps1 -Configuration Debug      # debug build
.\build.ps1 -SkipNative               # rebuild only the .NET app
```

The result is `dist/ImgConvert.exe`.

## How the single-file packaging works

- The native build uses vcpkg with the **`x64-windows-static`** triplet,
  which compiles every dependency (including x265, which is otherwise the
  largest source of runtime DLLs) into static `.lib` files. They get
  linked into `img_convert.dll`, so the DLL has *no* further dependencies
  beyond the system C runtime.
- The C# project lists `img_convert.dll` as `<Content>` so it ends up in
  the publish output.
- `dotnet publish` with `PublishSingleFile=true` +
  `IncludeNativeLibrariesForSelfExtract=true` packs the .NET runtime,
  every managed assembly, and `img_convert.dll` into a single `.exe` that
  self-extracts to a temp directory on first launch.

## Optional: app icon

To set a custom window/taskbar icon, drop a `app.ico` into
`app/Assets/app.ico`. The csproj picks it up automatically when present.

## License notes

- libheif and libde265 are LGPL (dynamic-linking-friendly, but static
  linking obliges you to ship object files or relink-ability).
- **x265** is **GPL v2**. By statically linking x265 you are inheriting
  GPL on the resulting binary. If you ship binaries to others you must
  either:
  - ship under GPL-compatible terms and provide source, **or**
  - swap the HEIF encoder for a non-GPL alternative (e.g. configure
    libheif to use kvazaar, which is BSD; remove `x265` from
    `native/vcpkg.json` and rebuild libheif with the `kvazaar` feature),
    **or**
  - drop HEIF encoding entirely and keep HEIF decode-only via
    libde265.
- libwebp, libpng, libjpeg-turbo, libtiff are permissive (BSD / IJG / etc.)
  and impose no obligations beyond preserving copyright notices.
