#include "Core/ProjectIO.h"
#include "Core/LayerManager.h"
#include "Core/Layer.h"
#include "Core/RasterLayer.h"
#include "Core/TextLayer.h"
#include "Render/TilePool.h"
#include "Render/RenderBackend.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <wincodec.h>

namespace VividPic {

static constexpr char VVP1_MAGIC[4] = {'V', 'V', 'P', '1'};
static constexpr char VVP2_MAGIC[4] = {'V', 'V', 'P', '2'};
static constexpr uint16_t VVP_VERSION = 2;
static constexpr char VVP_EOF[4] = {'V', 'E', 'N', 'D'};

static bool WriteU32(std::ostream& stream, uint32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

static bool WriteU16(std::ostream& stream, uint16_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

static bool WriteU8(std::ostream& stream, uint8_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

static bool WriteFloat(std::ostream& stream, float value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

static bool WriteString(std::ostream& stream, const String& str) {
    uint32_t byteLen = static_cast<uint32_t>(str.length() * sizeof(wchar_t));
    if (!WriteU32(stream, byteLen)) return false;
    if (byteLen > 0) {
        stream.write(reinterpret_cast<const char*>(str.c_str()), byteLen);
    }
    return stream.good();
}

static bool ReadU32(std::istream& stream, uint32_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

static bool ReadU16(std::istream& stream, uint16_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

static bool ReadU8(std::istream& stream, uint8_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

static bool ReadFloat(std::istream& stream, float& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

static bool ReadString(std::istream& stream, String& str) {
    uint32_t byteLen = 0;
    if (!ReadU32(stream, byteLen)) return false;
    if (byteLen == 0) {
        str.clear();
        return true;
    }
    std::vector<wchar_t> buffer;
    buffer.resize(byteLen / sizeof(wchar_t) + 1, 0);
    stream.read(reinterpret_cast<char*>(buffer.data()), byteLen);
    if (!stream.good()) return false;
    str.assign(buffer.data(), byteLen / sizeof(wchar_t));
    return true;
}

bool ProjectSerializer::SaveProject(const String& filePath, Project* project, LayerManager* layerManager) {
    if (!project || !layerManager) return false;

    int len = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back();
    }

    std::ofstream file(narrowPath, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        std::cerr << "[ProjectIO] Failed to open file for writing: " << narrowPath << std::endl;
        return false;
    }

    // Header
    file.write(VVP2_MAGIC, 4);
    WriteU16(file, VVP_VERSION);
    WriteU16(file, 0); // reserved

    // Canvas info
    const auto& canvas = project->GetCanvas();
    WriteU32(file, canvas.widthPx);
    WriteU32(file, canvas.heightPx);
    WriteFloat(file, canvas.dpi);
    WriteU8(file, static_cast<uint8_t>(canvas.colorMode));
    WriteU32(file, static_cast<uint32_t>(layerManager->GetActiveLayerIndex()));

    // Layers
    uint32_t layerCount = static_cast<uint32_t>(layerManager->GetLayerCount());
    WriteU32(file, layerCount);

    for (uint32_t i = 0; i < layerCount; ++i) {
        auto layer = layerManager->GetLayer(i);
        if (!layer) return false;
        if (!WriteLayer(file, layer.get())) {
            std::cerr << "[ProjectIO] Failed to write layer " << i << std::endl;
            return false;
        }
    }

    file.close();
    std::cout << "[ProjectIO] Saved project to " << narrowPath
              << " (" << layerCount << " layers, " << canvas.widthPx << "x" << canvas.heightPx << ")" << std::endl;
    return true;
}

Ref<Project> ProjectSerializer::LoadProject(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return nullptr;

    int len = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back();
    }

    std::ifstream file(narrowPath, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        std::cerr << "[ProjectIO] Failed to open file for reading: " << narrowPath << std::endl;
        return nullptr;
    }

    // Magic
    char magic[4];
    file.read(magic, 4);
    bool isV1 = (std::memcmp(magic, VVP1_MAGIC, 4) == 0);
    bool isV2 = (std::memcmp(magic, VVP2_MAGIC, 4) == 0);
    if (!isV1 && !isV2) {
        std::cerr << "[ProjectIO] Invalid file magic" << std::endl;
        return nullptr;
    }

    uint16_t version = 0;
    ReadU16(file, version);
    if ((isV1 && version != 1) || (isV2 && version != 2)) {
        std::cerr << "[ProjectIO] Unsupported version: " << version << std::endl;
        return nullptr;
    }

    uint16_t reserved = 0;
    ReadU16(file, reserved);

    // Canvas
    uint32_t canvasW = 0, canvasH = 0;
    float dpi = 350.0f;
    uint8_t colorMode = 0;
    uint32_t activeLayer = 0;

    if (!ReadU32(file, canvasW)) return nullptr;
    if (!ReadU32(file, canvasH)) return nullptr;
    if (!ReadFloat(file, dpi)) return nullptr;
    if (!ReadU8(file, colorMode)) return nullptr;
    if (!ReadU32(file, activeLayer)) return nullptr;

    // Reset and initialize layer manager
    layerManager->Initialize(canvasW, canvasH);

    uint32_t layerCount = 0;
    if (!ReadU32(file, layerCount)) return nullptr;

    for (uint32_t i = 0; i < layerCount; ++i) {
        auto layer = isV2 ? ReadLayerV2(file, canvasW, canvasH) : ReadLayerV1(file, canvasW, canvasH);
        if (!layer) {
            std::cerr << "[ProjectIO] Failed to read layer " << i << std::endl;
            return nullptr;
        }
        // Insert deserialized layer directly.
        // File stores bottom-to-top; LayerManager::m_layers[0] is bottom, back is top.
        // AddLayer(Ref<Layer>) pushes to back and sets active to back.
        layerManager->AddLayer(layer);
    }

    if (activeLayer < layerManager->GetLayerCount()) {
        layerManager->SetActiveLayer(activeLayer);
    }

    file.close();

    auto project = MakeRef<Project>(L"Untitled");
    auto& canvas = project->GetCanvas();
    canvas.widthPx = canvasW;
    canvas.heightPx = canvasH;
    canvas.dpi = dpi;
    canvas.colorMode = static_cast<ColorMode>(colorMode);

    std::cout << "[ProjectIO] Loaded project from " << narrowPath
              << " (" << layerCount << " layers, " << canvasW << "x" << canvasH << ")" << std::endl;
    return project;
}

bool ProjectSerializer::ExportPNG(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return false;

    uint32_t width = layerManager->GetCanvasWidth();
    uint32_t height = layerManager->GetCanvasHeight();
    if (width == 0 || height == 0) return false;

    // Composite to buffer
    std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * 4);
    layerManager->CompositeToBuffer(buffer.data(), width, height);

    // Use WIC to encode PNG
    auto& backend = Render::RenderBackend::GetInstance();
    IWICImagingFactory* wicFactory = backend.GetWicFactory();
    if (!wicFactory) {
        std::cerr << "[ProjectIO] WIC factory not available" << std::endl;
        return false;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath;
    if (len > 0) {
        narrowPath.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, narrowPath.data(), len, nullptr, nullptr);
        narrowPath.pop_back();
    }

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

    // Our buffer is BGRA (from CompositeToBuffer), write it directly
    uint32_t stride = width * 4;
    hr = frame->WritePixels(height, stride, static_cast<UINT>(buffer.size()), buffer.data());
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

    if (FAILED(hr)) return false;

    std::cout << "[ProjectIO] Exported PNG to " << narrowPath << std::endl;
    return true;
}

bool ProjectSerializer::WriteLayer(std::ostream& stream, Layer* layer) {
    if (!layer) return false;

    if (!WriteString(stream, layer->GetName())) return false;
    if (!WriteU8(stream, static_cast<uint8_t>(layer->GetType()))) return false;
    if (!WriteU8(stream, static_cast<uint8_t>(layer->GetBlendMode()))) return false;
    if (!WriteU8(stream, layer->GetOpacity())) return false;
    if (!WriteU8(stream, layer->IsVisible() ? 1 : 0)) return false;
    if (!WriteU8(stream, layer->IsLocked() ? 1 : 0)) return false;
    if (!WriteU8(stream, layer->IsProtectAlpha() ? 1 : 0)) return false;
    if (!WriteU32(stream, layer->GetGridWidth())) return false;
    if (!WriteU32(stream, layer->GetGridHeight())) return false;

    // Count non-null tiles
    uint32_t tileCount = 0;
    for (uint32_t gy = 0; gy < layer->GetGridHeight(); ++gy) {
        for (uint32_t gx = 0; gx < layer->GetGridWidth(); ++gx) {
            if (layer->HasTile(gx, gy)) tileCount++;
        }
    }
    if (!WriteU32(stream, tileCount)) return false;

    for (uint32_t gy = 0; gy < layer->GetGridHeight(); ++gy) {
        for (uint32_t gx = 0; gx < layer->GetGridWidth(); ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;
            if (!WriteU32(stream, gx)) return false;
            if (!WriteU32(stream, gy)) return false;
            stream.write(reinterpret_cast<const char*>(tile->data), Render::TILE_BYTES);
            if (!stream.good()) return false;
        }
    }

    // VVP v2: payload for text/vector layers
    auto payload = layer->SerializePayload();
    uint8_t payloadType = static_cast<uint8_t>(layer->GetType());
    if (!WriteU8(stream, payloadType)) return false;
    if (!WriteU32(stream, static_cast<uint32_t>(payload.size()))) return false;
    if (!payload.empty()) {
        stream.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        if (!stream.good()) return false;
    }

    return true;
}

static Ref<Layer> CreateLayerFromType(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight) {
    switch (type) {
        case LayerType::Text:
            return MakeRef<TextLayer>(name, canvasWidth, canvasHeight);
        default:
            return MakeRef<RasterLayer>(name, type, canvasWidth, canvasHeight);
    }
}

Ref<Layer> ProjectSerializer::ReadLayerV1(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight) {
    String name;
    if (!ReadString(stream, name)) return nullptr;

    uint8_t typeVal = 0, blendVal = 0, opacity = 255, visible = 1, locked = 0, protectAlpha = 0;
    if (!ReadU8(stream, typeVal)) return nullptr;
    if (!ReadU8(stream, blendVal)) return nullptr;
    if (!ReadU8(stream, opacity)) return nullptr;
    if (!ReadU8(stream, visible)) return nullptr;
    if (!ReadU8(stream, locked)) return nullptr;
    if (!ReadU8(stream, protectAlpha)) return nullptr;

    uint32_t gridW = 0, gridH = 0, tileCount = 0;
    if (!ReadU32(stream, gridW)) return nullptr;
    if (!ReadU32(stream, gridH)) return nullptr;
    if (!ReadU32(stream, tileCount)) return nullptr;

    auto layer = CreateLayerFromType(name, static_cast<LayerType>(typeVal), canvasWidth, canvasHeight);
    layer->SetBlendMode(static_cast<BlendMode>(blendVal));
    layer->SetOpacity(opacity);
    layer->SetVisible(visible != 0);
    layer->SetLocked(locked != 0);
    layer->SetProtectAlpha(protectAlpha != 0);

    for (uint32_t i = 0; i < tileCount; ++i) {
        uint32_t gx = 0, gy = 0;
        if (!ReadU32(stream, gx)) return nullptr;
        if (!ReadU32(stream, gy)) return nullptr;

        Render::Tile tmpTile;
        stream.read(reinterpret_cast<char*>(tmpTile.data), Render::TILE_BYTES);
        if (!stream.good()) return nullptr;
        layer->ImportTile(gx, gy, tmpTile.data);
    }

    return layer;
}

Ref<Layer> ProjectSerializer::ReadLayerV2(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight) {
    String name;
    if (!ReadString(stream, name)) return nullptr;

    uint8_t typeVal = 0, blendVal = 0, opacity = 255, visible = 1, locked = 0, protectAlpha = 0;
    if (!ReadU8(stream, typeVal)) return nullptr;
    if (!ReadU8(stream, blendVal)) return nullptr;
    if (!ReadU8(stream, opacity)) return nullptr;
    if (!ReadU8(stream, visible)) return nullptr;
    if (!ReadU8(stream, locked)) return nullptr;
    if (!ReadU8(stream, protectAlpha)) return nullptr;

    uint32_t gridW = 0, gridH = 0, tileCount = 0;
    if (!ReadU32(stream, gridW)) return nullptr;
    if (!ReadU32(stream, gridH)) return nullptr;
    if (!ReadU32(stream, tileCount)) return nullptr;

    auto layer = CreateLayerFromType(name, static_cast<LayerType>(typeVal), canvasWidth, canvasHeight);
    layer->SetBlendMode(static_cast<BlendMode>(blendVal));
    layer->SetOpacity(opacity);
    layer->SetVisible(visible != 0);
    layer->SetLocked(locked != 0);
    layer->SetProtectAlpha(protectAlpha != 0);

    for (uint32_t i = 0; i < tileCount; ++i) {
        uint32_t gx = 0, gy = 0;
        if (!ReadU32(stream, gx)) return nullptr;
        if (!ReadU32(stream, gy)) return nullptr;

        Render::Tile tmpTile;
        stream.read(reinterpret_cast<char*>(tmpTile.data), Render::TILE_BYTES);
        if (!stream.good()) return nullptr;
        layer->ImportTile(gx, gy, tmpTile.data);
    }

    // VVP v2: read payload
    uint8_t payloadType = 0;
    if (!ReadU8(stream, payloadType)) return nullptr;
    uint32_t payloadLen = 0;
    if (!ReadU32(stream, payloadLen)) return nullptr;
    if (payloadLen > 0) {
        std::vector<uint8_t> payload(payloadLen);
        stream.read(reinterpret_cast<char*>(payload.data()), payloadLen);
        if (!stream.good()) return nullptr;
        layer->DeserializePayload(payload.data(), payloadLen);
    }

    return layer;
}

} // namespace VividPic
