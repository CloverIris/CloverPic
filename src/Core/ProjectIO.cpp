#include "Core/ProjectIO.h"
#include "Core/Layer.h"
#include "Core/LayerManager.h"
#include "Core/RasterLayer.h"
#include "Core/Services/CoreServices.h"
#include "Core/TextLayer.h"
#include "Core/Render/TilePool.h"
#include <cstring>
#include <sstream>

namespace CloverPic {

namespace {

constexpr char VVP3_MAGIC[4] = {'V', 'V', 'P', '3'};
constexpr uint16_t VVP_VERSION = 3;

bool WriteU32(std::ostream& stream, uint32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

bool WriteU16(std::ostream& stream, uint16_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

bool WriteU8(std::ostream& stream, uint8_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

bool WriteFloat(std::ostream& stream, float value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return stream.good();
}

bool WriteString(std::ostream& stream, const String& str) {
    if (!WriteU32(stream, static_cast<uint32_t>(str.size()))) return false;
    for (wchar_t ch : str) {
        if (!WriteU32(stream, static_cast<uint32_t>(ch))) return false;
    }
    return stream.good();
}

bool ReadU32(std::istream& stream, uint32_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool ReadU16(std::istream& stream, uint16_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool ReadU8(std::istream& stream, uint8_t& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool ReadFloat(std::istream& stream, float& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return stream.good();
}

bool ReadString(std::istream& stream, String& str) {
    uint32_t length = 0;
    if (!ReadU32(stream, length)) return false;
    str.clear();
    str.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
        uint32_t codepoint = 0;
        if (!ReadU32(stream, codepoint)) return false;
        str.push_back(static_cast<wchar_t>(codepoint));
    }
    return true;
}

Ref<Layer> CreateLayerFromType(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight) {
    switch (type) {
        case LayerType::Text:
            return MakeRef<TextLayer>(name, canvasWidth, canvasHeight);
        default:
            return MakeRef<RasterLayer>(name, type, canvasWidth, canvasHeight);
    }
}

std::vector<uint8_t> StreamToBytes(const std::ostringstream& stream) {
    const std::string data = stream.str();
    return std::vector<uint8_t>(data.begin(), data.end());
}

std::istringstream BytesToStream(const std::vector<uint8_t>& bytes) {
    return std::istringstream(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()), std::ios::binary);
}

} // namespace

bool ProjectSerializer::SaveProject(const String& filePath, Project* project, LayerManager* layerManager) {
    if (!project || !layerManager) return false;
    auto* fileSystem = CoreServices::GetFileSystem();
    if (!fileSystem) return false;

    std::ostringstream stream(std::ios::binary);
    stream.write(VVP3_MAGIC, 4);
    WriteU16(stream, VVP_VERSION);
    WriteU16(stream, 0);

    const auto& canvas = project->GetCanvas();
    WriteU32(stream, canvas.widthPx);
    WriteU32(stream, canvas.heightPx);
    WriteFloat(stream, canvas.dpi);
    WriteU8(stream, static_cast<uint8_t>(canvas.colorMode));
    WriteU8(stream, canvas.transparent ? 1 : 0);
    WriteU8(stream, static_cast<uint8_t>(canvas.initialLayerType));
    WriteU8(stream, 0);
    WriteU32(stream, static_cast<uint32_t>(layerManager->GetActiveLayerIndex()));

    const uint32_t layerCount = static_cast<uint32_t>(layerManager->GetLayerCount());
    WriteU32(stream, layerCount);
    for (uint32_t i = 0; i < layerCount; ++i) {
        auto layer = layerManager->GetLayer(i);
        if (!layer || !WriteLayer(stream, layer.get())) return false;
    }

    if (!stream.good()) return false;
    return fileSystem->WriteFileBytes(filePath, StreamToBytes(stream));
}

Ref<Project> ProjectSerializer::LoadProject(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return nullptr;
    auto* fileSystem = CoreServices::GetFileSystem();
    if (!fileSystem) return nullptr;

    std::vector<uint8_t> bytes;
    if (!fileSystem->ReadFileBytes(filePath, bytes) || bytes.empty()) return nullptr;
    auto stream = BytesToStream(bytes);

    char magic[4] = {};
    stream.read(magic, 4);
    if (!stream.good() || std::memcmp(magic, VVP3_MAGIC, 4) != 0) {
        return nullptr;
    }

    uint16_t version = 0;
    uint16_t reserved = 0;
    if (!ReadU16(stream, version) || version != VVP_VERSION || !ReadU16(stream, reserved)) {
        return nullptr;
    }

    uint32_t canvasW = 0;
    uint32_t canvasH = 0;
    float dpi = 350.0f;
    uint8_t colorMode = 0;
    uint8_t transparent = 1;
    uint8_t initialLayerType = static_cast<uint8_t>(LayerType::Transparent);
    uint8_t reservedByte = 0;
    uint32_t activeLayer = 0;
    if (!ReadU32(stream, canvasW) || !ReadU32(stream, canvasH) || !ReadFloat(stream, dpi) ||
        !ReadU8(stream, colorMode) || !ReadU8(stream, transparent) ||
        !ReadU8(stream, initialLayerType) || !ReadU8(stream, reservedByte) || !ReadU32(stream, activeLayer)) {
        return nullptr;
    }

    layerManager->Initialize(canvasW, canvasH);
    uint32_t layerCount = 0;
    if (!ReadU32(stream, layerCount)) return nullptr;
    for (uint32_t i = 0; i < layerCount; ++i) {
        auto layer = ReadLayer(stream, canvasW, canvasH);
        if (!layer) return nullptr;
        layerManager->AddLayer(layer);
    }
    if (activeLayer < layerManager->GetLayerCount()) {
        layerManager->SetActiveLayer(activeLayer);
    }

    auto project = MakeRef<Project>(L"Untitled");
    auto& canvas = project->GetCanvas();
    canvas.widthPx = canvasW;
    canvas.heightPx = canvasH;
    canvas.dpi = dpi;
    canvas.colorMode = static_cast<ColorMode>(colorMode);
    canvas.transparent = transparent != 0;
    canvas.initialLayerType = static_cast<LayerType>(initialLayerType);
    return project;
}

bool ProjectSerializer::ExportPNG(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return false;
    auto* encoder = CoreServices::GetImageEncoder();
    if (!encoder) return false;

    const uint32_t width = layerManager->GetCanvasWidth();
    const uint32_t height = layerManager->GetCanvasHeight();
    if (width == 0 || height == 0) return false;

    std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * 4);
    layerManager->CompositeToBuffer(buffer.data(), width, height);
    return encoder->EncodePngBgra(filePath, width, height, buffer);
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
            if (!WriteU32(stream, gx) || !WriteU32(stream, gy)) return false;
            stream.write(reinterpret_cast<const char*>(tile->data), Render::TILE_BYTES);
            if (!stream.good()) return false;
        }
    }

    const auto payload = layer->SerializePayload();
    if (!WriteU8(stream, static_cast<uint8_t>(layer->GetType()))) return false;
    if (!WriteU32(stream, static_cast<uint32_t>(payload.size()))) return false;
    if (!payload.empty()) {
        stream.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        if (!stream.good()) return false;
    }

    return true;
}

Ref<Layer> ProjectSerializer::ReadLayer(std::istream& stream, uint32_t canvasWidth, uint32_t canvasHeight) {
    String name;
    if (!ReadString(stream, name)) return nullptr;

    uint8_t typeVal = 0;
    uint8_t blendVal = 0;
    uint8_t opacity = 255;
    uint8_t visible = 1;
    uint8_t locked = 0;
    uint8_t protectAlpha = 0;
    if (!ReadU8(stream, typeVal) || !ReadU8(stream, blendVal) || !ReadU8(stream, opacity) ||
        !ReadU8(stream, visible) || !ReadU8(stream, locked) || !ReadU8(stream, protectAlpha)) {
        return nullptr;
    }

    uint32_t gridW = 0;
    uint32_t gridH = 0;
    uint32_t tileCount = 0;
    if (!ReadU32(stream, gridW) || !ReadU32(stream, gridH) || !ReadU32(stream, tileCount)) return nullptr;

    auto layer = CreateLayerFromType(name, static_cast<LayerType>(typeVal), canvasWidth, canvasHeight);
    layer->SetBlendMode(static_cast<BlendMode>(blendVal));
    layer->SetOpacity(opacity);
    layer->SetVisible(visible != 0);
    layer->SetLocked(locked != 0);
    layer->SetProtectAlpha(protectAlpha != 0);

    for (uint32_t i = 0; i < tileCount; ++i) {
        uint32_t gx = 0;
        uint32_t gy = 0;
        if (!ReadU32(stream, gx) || !ReadU32(stream, gy)) return nullptr;

        Render::Tile tmpTile;
        stream.read(reinterpret_cast<char*>(tmpTile.data), Render::TILE_BYTES);
        if (!stream.good()) return nullptr;
        layer->ImportTile(gx, gy, tmpTile.data);
    }

    uint8_t payloadType = 0;
    uint32_t payloadLen = 0;
    if (!ReadU8(stream, payloadType) || !ReadU32(stream, payloadLen)) return nullptr;
    if (payloadLen > 0) {
        std::vector<uint8_t> payload(payloadLen);
        stream.read(reinterpret_cast<char*>(payload.data()), payloadLen);
        if (!stream.good()) return nullptr;
        layer->DeserializePayload(payload.data(), payloadLen);
    }

    return layer;
}

} // namespace CloverPic
