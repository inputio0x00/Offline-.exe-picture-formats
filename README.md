# Offline Image Converter

Offline Image Converter is a Windows-oriented C++ application scaffold for a single-file `.exe` batch converter with a lightweight modern Win32 interface. It is designed for offline conversion between HEIF/HEIC, JPEG, PNG, WEBP, and TIFF.

## Feasibility

Yes, the project is feasible, with one important packaging caveat: **guaranteed bidirectional HEIF and WEBP support requires shipping or statically linking codecs such as libheif, libde265/x265/AOM, and libwebp**. The current implementation uses Windows Imaging Component (WIC), which keeps the app small and offline but depends on locally available Windows codecs for HEIF and WEBP. JPEG, PNG, and TIFF are broadly available through WIC on supported Windows versions.

A production build can still be packaged as a single standalone executable by statically linking the C++ runtime and codec libraries. The trade-off is executable size, patent/licensing review for HEIF encoders, and a more involved dependency build pipeline.

## Current capabilities

- C++20 backend model for target formats, file filtering, output naming, and batch conversion planning.
- Native Windows UI with a drop-enabled file list, target format selector, quality slider, output folder field, and batch conversion button.
- WIC-based conversion pipeline for offline local files.
- Encoder options that retain format strengths where supported by the installed codec:
  - JPEG/WEBP/HEIF quality setting.
  - TIFF ZIP compression.
  - PNG lossless output path.
- Collision-safe output naming when overwrite is disabled.

## Format strategy

| Format | Intended algorithmic advantage | Production recommendation |
| --- | --- | --- |
| HEIF/HEIC | High compression efficiency and modern codecs | Bundle `libheif` plus chosen HEVC/AV1 backend for guaranteed offline encode/decode. |
| WEBP | Lossy and lossless web delivery with alpha support | Bundle `libwebp` for predictable quality/lossless controls. |
| TIFF | Archival workflows, multi-page potential, lossless compression | Bundle `libtiff` when multi-page, tiled, or specialized compression support is required. |
| PNG | Lossless compression and alpha | WIC is acceptable for basic PNG; `libpng`/`zlib` gives deeper compression tuning. |
| JPEG | Universal lossy photo format | WIC is acceptable; `libjpeg-turbo` is faster for high-volume batches. |

## Build

### Windows `.exe`

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The executable is produced at:

```text
build\Release\OfflineImageConverter.exe
```

### Linux/macOS smoke build

The GUI target is Windows-only. On non-Windows hosts, the build produces a small smoke executable and the unit tests for the portable core.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap to full production support

1. Add a codec abstraction next to the WIC converter.
2. Integrate statically built `libheif`, `libwebp`, `libtiff`, `libpng`, `zlib`, and optionally `libjpeg-turbo`.
3. Preserve color profiles, EXIF/XMP metadata, orientation, alpha, bit depth, and animation/multi-page semantics per format.
4. Move conversion work to a background thread and add cancellation/progress per file.
5. Add installer-free release packaging and CI-built Windows artifacts.
