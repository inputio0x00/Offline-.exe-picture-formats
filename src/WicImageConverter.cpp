#include "WicImageConverter.h"

#ifdef _WIN32
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace oic {
namespace {
GUID containerGuid(ImageFormat format) {
    switch (format) {
    case ImageFormat::Heif:
        return GUID_ContainerFormatHeif;
    case ImageFormat::Jpeg:
        return GUID_ContainerFormatJpeg;
    case ImageFormat::Png:
        return GUID_ContainerFormatPng;
    case ImageFormat::Webp:
        return GUID_ContainerFormatWebp;
    case ImageFormat::Tiff:
        return GUID_ContainerFormatTiff;
    }
    return GUID_ContainerFormatPng;
}

void throwIfFailed(HRESULT hr, const char* operation) {
    if (FAILED(hr)) {
        throw std::runtime_error(operation);
    }
}

void setEncoderOptions(IPropertyBag2* bag, ImageFormat format, const ConversionOptions& options) {
    if (!bag) {
        return;
    }

    if (format == ImageFormat::Jpeg || format == ImageFormat::Webp || format == ImageFormat::Heif) {
        PROPBAG2 property{};
        property.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = static_cast<float>(options.quality) / 100.0f;
        bag->Write(1, &property, &value);
        VariantClear(&value);
    }

    if (format == ImageFormat::Tiff) {
        PROPBAG2 property{};
        property.pstrName = const_cast<LPOLESTR>(L"TiffCompressionMethod");
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_UI1;
        value.bVal = WICTiffCompressionZIP;
        bag->Write(1, &property, &value);
        VariantClear(&value);
    }

    if (format == ImageFormat::Png) {
        PROPBAG2 property{};
        property.pstrName = const_cast<LPOLESTR>(L"InterlaceOption");
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        bag->Write(1, &property, &value);
        VariantClear(&value);
    }
}
} // namespace

WicImageConverter::WicImageConverter() {
    throwIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "Initialize COM");
    IWICImagingFactory* factory = nullptr;
    throwIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)),
                  "Create WIC factory");
    factory_ = factory;
}

WicImageConverter::~WicImageConverter() {
    if (factory_) {
        static_cast<IWICImagingFactory*>(factory_)->Release();
    }
    CoUninitialize();
}

bool WicImageConverter::canEncode(ImageFormat format) const {
    ComPtr<IWICBitmapEncoder> encoder;
    return SUCCEEDED(static_cast<IWICImagingFactory*>(factory_)->CreateEncoder(containerGuid(format), nullptr, &encoder));
}

bool WicImageConverter::canDecode(ImageFormat format) const {
    ComPtr<IEnumUnknown> enumerator;
    if (FAILED(static_cast<IWICImagingFactory*>(factory_)->CreateComponentEnumerator(WICDecoder, WICComponentEnumerateDefault, &enumerator))) {
        return false;
    }
    ULONG fetched = 0;
    ComPtr<IUnknown> unknown;
    while (enumerator->Next(1, &unknown, &fetched) == S_OK) {
        ComPtr<IWICBitmapDecoderInfo> info;
        if (SUCCEEDED(unknown.As(&info))) {
            GUID guid{};
            if (SUCCEEDED(info->GetContainerFormat(&guid)) && guid == containerGuid(format)) {
                return true;
            }
        }
        unknown.Reset();
    }
    return false;
}

void WicImageConverter::convertBatch(const ConversionPlan& plan, const ConversionOptions& options, const ProgressCallback& onProgress) {
    IWICImagingFactory* factory = static_cast<IWICImagingFactory*>(factory_);
    std::size_t completed = 0;

    for (const auto& item : plan.items) {
        if (onProgress) {
            onProgress({completed, plan.items.size(), item.inputPath.wstring()});
        }

        ComPtr<IWICBitmapDecoder> decoder;
        throwIfFailed(factory->CreateDecoderFromFilename(item.inputPath.c_str(), nullptr, GENERIC_READ,
                                                         WICDecodeMetadataCacheOnLoad, &decoder),
                      "Decode source image");

        ComPtr<IWICBitmapFrameDecode> sourceFrame;
        throwIfFailed(decoder->GetFrame(0, &sourceFrame), "Read first image frame");

        ComPtr<IWICFormatConverter> converter;
        throwIfFailed(factory->CreateFormatConverter(&converter), "Create pixel format converter");
        throwIfFailed(converter->Initialize(sourceFrame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                            WICBitmapDitherTypeNone, nullptr, 0.0,
                                            WICBitmapPaletteTypeCustom),
                      "Convert source pixels");

        std::filesystem::create_directories(item.outputPath.parent_path());
        ComPtr<IWICStream> stream;
        throwIfFailed(factory->CreateStream(&stream), "Create output stream");
        throwIfFailed(stream->InitializeFromFilename(item.outputPath.c_str(), GENERIC_WRITE), "Open output image");

        ComPtr<IWICBitmapEncoder> encoder;
        throwIfFailed(factory->CreateEncoder(containerGuid(options.targetFormat), nullptr, &encoder), "Create target encoder");
        throwIfFailed(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Initialize encoder");

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> propertyBag;
        throwIfFailed(encoder->CreateNewFrame(&frame, &propertyBag), "Create target frame");
        setEncoderOptions(propertyBag.Get(), options.targetFormat, options);
        throwIfFailed(frame->Initialize(propertyBag.Get()), "Initialize target frame");

        UINT width = 0;
        UINT height = 0;
        throwIfFailed(sourceFrame->GetSize(&width, &height), "Read source dimensions");
        throwIfFailed(frame->SetSize(width, height), "Set target dimensions");

        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppPBGRA;
        throwIfFailed(frame->SetPixelFormat(&pixelFormat), "Set target pixel format");
        throwIfFailed(frame->WriteSource(converter.Get(), nullptr), "Write target pixels");
        throwIfFailed(frame->Commit(), "Commit target frame");
        throwIfFailed(encoder->Commit(), "Commit target image");

        ++completed;
    }

    if (onProgress) {
        onProgress({completed, plan.items.size(), L""});
    }
}

} // namespace oic
#else
namespace oic {
WicImageConverter::WicImageConverter() = default;
WicImageConverter::~WicImageConverter() = default;
void WicImageConverter::convertBatch(const ConversionPlan&, const ConversionOptions&, const ProgressCallback&) {}
bool WicImageConverter::canEncode(ImageFormat) const { return false; }
bool WicImageConverter::canDecode(ImageFormat) const { return false; }
} // namespace oic
#endif
