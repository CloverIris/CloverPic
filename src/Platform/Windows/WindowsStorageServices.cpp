#include "Platform/Windows/WindowsStorageServices.h"
#include "Platform/Windows/RenderBackend.h"
#include <fstream>
#include <string>
#include <cstring>
#include <wincodec.h>
#include <windows.h>
#include <objidl.h>

namespace CloverPic {

namespace {

std::string ToUtf8Path(const String& filePath) {
    int len = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back();
    }
    return narrowPath;
}

} // namespace

bool WindowsFileSystem::ReadFileBytes(const String& filePath, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    std::ifstream file(ToUtf8Path(filePath), std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    outBytes.resize(static_cast<size_t>(size));
    if (!outBytes.empty()) {
        file.read(reinterpret_cast<char*>(outBytes.data()), size);
    }
    return file.good() || file.eof();
}

bool WindowsFileSystem::WriteFileBytes(const String& filePath, const std::vector<uint8_t>& bytes) {
    std::ofstream file(ToUtf8Path(filePath), std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        return false;
    }
    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return file.good();
}

bool WindowsImageCodec::EncodeDisplayPngBgra8ToFile(const String& filePath, uint32_t width, uint32_t height,
                                                    const std::vector<uint8_t>& bgraPixels) {
    if (width == 0 || height == 0 || bgraPixels.size() < static_cast<size_t>(width) * height * 4) {
        return false;
    }

    auto& backend = Render::RenderBackend::GetInstance();
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) return false;

    IWICStream* stream = nullptr;
    HRESULT hr = wicFactory->CreateStream(&stream);
    if (FAILED(hr) || !stream) return false;

    hr = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        stream->Release();
        return false;
    }

    IWICBitmapEncoder* encoder = nullptr;
    hr = wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr) || !encoder) {
        stream->Release();
        return false;
    }

    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        encoder->Release();
        stream->Release();
        return false;
    }

    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr) || !frame) {
        encoder->Release();
        stream->Release();
        return false;
    }

    hr = frame->Initialize(props);
    if (props) props->Release();
    if (FAILED(hr)) {
        frame->Release();
        encoder->Release();
        stream->Release();
        return false;
    }

    hr = frame->SetSize(width, height);
    if (FAILED(hr)) {
        frame->Release();
        encoder->Release();
        stream->Release();
        return false;
    }

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) {
        frame->Release();
        encoder->Release();
        stream->Release();
        return false;
    }

    const uint32_t stride = width * 4;
    hr = frame->WritePixels(height, stride, static_cast<UINT>(bgraPixels.size()),
                            const_cast<uint8_t*>(bgraPixels.data()));
    if (FAILED(hr)) {
        frame->Release();
        encoder->Release();
        stream->Release();
        return false;
    }

    hr = frame->Commit();
    if (FAILED(hr)) {
        frame->Release();
        encoder->Release();
        stream->Release();
        return false;
    }

    hr = encoder->Commit();
    frame->Release();
    encoder->Release();
    stream->Release();
    return SUCCEEDED(hr);
}

bool WindowsImageCodec::EncodePngRgba16ToBytes(uint32_t width, uint32_t height,
                                               const std::vector<uint16_t>& rgba16Pixels,
                                               std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    if (width == 0 || height == 0 || rgba16Pixels.size() < static_cast<size_t>(width) * height * 4) {
        return false;
    }

    auto& backend = Render::RenderBackend::GetInstance();
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) return false;

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) || !stream) return false;

    IWICBitmapEncoder* encoder = nullptr;
    HRESULT hr = wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr) || !encoder) {
        stream->Release();
        return false;
    }
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        encoder->Release();
        stream->Release();
        return false;
    }

    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr) || !frame) {
        encoder->Release();
        stream->Release();
        return false;
    }
    hr = frame->Initialize(props);
    if (props) props->Release();
    if (SUCCEEDED(hr)) hr = frame->SetSize(width, height);
    WICPixelFormatGUID format = GUID_WICPixelFormat64bppRGBA;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    const uint32_t stride = width * 8;
    const uint32_t byteSize = stride * height;
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(height, stride, byteSize,
                                reinterpret_cast<BYTE*>(const_cast<uint16_t*>(rgba16Pixels.data())));
    }
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();

    if (SUCCEEDED(hr)) {
        HGLOBAL global = nullptr;
        if (SUCCEEDED(GetHGlobalFromStream(stream, &global)) && global) {
            const SIZE_T size = GlobalSize(global);
            if (const void* data = GlobalLock(global)) {
                const auto* bytes = static_cast<const uint8_t*>(data);
                outBytes.assign(bytes, bytes + size);
                GlobalUnlock(global);
            }
        }
    }

    frame->Release();
    encoder->Release();
    stream->Release();
    return SUCCEEDED(hr) && !outBytes.empty();
}

bool WindowsImageCodec::DecodePngRgba16FromBytes(const std::vector<uint8_t>& bytes,
                                                 uint32_t& outWidth,
                                                 uint32_t& outHeight,
                                                 std::vector<uint16_t>& outRgba16Pixels) {
    outWidth = 0;
    outHeight = 0;
    outRgba16Pixels.clear();
    if (bytes.empty()) return false;

    auto& backend = Render::RenderBackend::GetInstance();
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) return false;

    HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!global) return false;
    void* target = GlobalLock(global);
    if (!target) {
        GlobalFree(global);
        return false;
    }
    std::memcpy(target, bytes.data(), bytes.size());
    GlobalUnlock(global);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(global, TRUE, &stream)) || !stream) {
        GlobalFree(global);
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = wicFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    IWICBitmapFrameDecode* frame = nullptr;
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = frame->GetSize(&outWidth, &outHeight);

    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(hr)) hr = wicFactory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat64bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0f,
                                   WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(hr)) {
        outRgba16Pixels.resize(static_cast<size_t>(outWidth) * outHeight * 4);
        const uint32_t stride = outWidth * 8;
        const uint32_t byteSize = stride * outHeight;
        hr = converter->CopyPixels(nullptr, stride, byteSize,
                                   reinterpret_cast<BYTE*>(outRgba16Pixels.data()));
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    stream->Release();
    return SUCCEEDED(hr) && outWidth > 0 && outHeight > 0 && !outRgba16Pixels.empty();
}

} // namespace CloverPic
