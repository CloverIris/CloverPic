#include "Core/Document/ProjectIO.h"
#include "Core/Layers/Layer.h"
#include "Core/Layers/LayerManager.h"
#include "Core/Layers/RasterLayer.h"
#include "Core/Services/CoreServices.h"
#include "Core/Layers/TextLayer.h"
#include "Core/Render/TilePool.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace CloverPic {

namespace {

constexpr char CloverMagic[8] = {'C', 'L', 'V', 'P', 'I', 'C', '1', '0'};
constexpr uint16_t CloverVersion = 1;
constexpr uint32_t HeaderBytes = 24;

enum class ResourceKind : uint32_t {
    RasterPng16 = 1,
    Svg = 2,
    Payload = 3
};

struct LayerManifestEntry {
    uint64_t layerIndex = 0;
    String name;
    LayerType type = LayerType::Transparent;
    BlendMode blendMode = BlendMode::Normal;
    uint8_t opacity = 255;
    bool visible = true;
    bool locked = false;
    bool protectAlpha = false;
    String resourceId;
    ResourceKind resourceKind = ResourceKind::Payload;
    PixelFormat pixelFormat = PixelFormat::Rgba10Unorm;
    uint16_t bitDepth = 10;
    uint64_t resourceSize = 0;
};

struct ResourceBlob {
    String id;
    uint64_t layerIndex = 0;
    ResourceKind kind = ResourceKind::Payload;
    PixelFormat pixelFormat = PixelFormat::Rgba10Unorm;
    uint16_t bitDepth = 10;
    std::vector<uint8_t> bytes;
    LayerManifestEntry metadata;
    uint64_t chunkOffset = 0;
    uint32_t crc = 0;
};

bool WriteRaw(std::ostream& stream, const void* data, size_t size) {
    if (size == 0) return stream.good();
    stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return stream.good();
}

bool WriteU8(std::ostream& stream, uint8_t value) { return WriteRaw(stream, &value, sizeof(value)); }
bool WriteU16(std::ostream& stream, uint16_t value) { return WriteRaw(stream, &value, sizeof(value)); }
bool WriteU32(std::ostream& stream, uint32_t value) { return WriteRaw(stream, &value, sizeof(value)); }
bool WriteU64(std::ostream& stream, uint64_t value) { return WriteRaw(stream, &value, sizeof(value)); }
bool WriteFloat(std::ostream& stream, float value) { return WriteRaw(stream, &value, sizeof(value)); }

bool WriteString(std::ostream& stream, const String& str) {
    if (!WriteU64(stream, static_cast<uint64_t>(str.size()))) return false;
    for (wchar_t ch : str) {
        if (!WriteU32(stream, static_cast<uint32_t>(ch))) return false;
    }
    return stream.good();
}

bool WriteBytes(std::ostream& stream, const std::vector<uint8_t>& bytes) {
    if (!WriteU64(stream, static_cast<uint64_t>(bytes.size()))) return false;
    return WriteRaw(stream, bytes.data(), bytes.size());
}

bool ReadRaw(std::istream& stream, void* data, size_t size) {
    if (size == 0) return stream.good();
    stream.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    return stream.good();
}

bool ReadU8(std::istream& stream, uint8_t& value) { return ReadRaw(stream, &value, sizeof(value)); }
bool ReadU16(std::istream& stream, uint16_t& value) { return ReadRaw(stream, &value, sizeof(value)); }
bool ReadU32(std::istream& stream, uint32_t& value) { return ReadRaw(stream, &value, sizeof(value)); }
bool ReadU64(std::istream& stream, uint64_t& value) { return ReadRaw(stream, &value, sizeof(value)); }
bool ReadFloat(std::istream& stream, float& value) { return ReadRaw(stream, &value, sizeof(value)); }

bool ReadString(std::istream& stream, String& str) {
    uint64_t length = 0;
    if (!ReadU64(stream, length) || length > 1'000'000) return false;
    str.clear();
    str.reserve(static_cast<size_t>(length));
    for (uint64_t i = 0; i < length; ++i) {
        uint32_t codepoint = 0;
        if (!ReadU32(stream, codepoint)) return false;
        str.push_back(static_cast<wchar_t>(codepoint));
    }
    return true;
}

bool ReadBytes(std::istream& stream, std::vector<uint8_t>& bytes) {
    uint64_t size = 0;
    if (!ReadU64(stream, size) || size > (1ull << 34)) return false;
    bytes.resize(static_cast<size_t>(size));
    return ReadRaw(stream, bytes.data(), bytes.size());
}

std::vector<uint8_t> StreamToBytes(const std::ostringstream& stream) {
    const std::string data = stream.str();
    return std::vector<uint8_t>(data.begin(), data.end());
}

std::istringstream BytesToStream(const std::vector<uint8_t>& bytes) {
    return std::istringstream(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()), std::ios::binary);
}

uint32_t Crc32Like(const std::vector<uint8_t>& bytes) {
    uint32_t hash = 2166136261u;
    for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

uint16_t Expand10To16(uint16_t value) {
    return static_cast<uint16_t>((static_cast<uint32_t>(std::min<uint16_t>(value, 1023)) * 65535u + 511u) / 1023u);
}

uint16_t Collapse16To10(uint16_t value) {
    return static_cast<uint16_t>((static_cast<uint32_t>(value) * 1023u + 32767u) / 65535u);
}

Ref<Layer> CreateLayerFromType(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight) {
    switch (type) {
        case LayerType::Text:
            return MakeRef<TextLayer>(name, canvasWidth, canvasHeight);
        default:
            return MakeRef<RasterLayer>(name, type, canvasWidth, canvasHeight);
    }
}

bool IsRasterLayerType(LayerType type) {
    return type == LayerType::Color || type == LayerType::Grayscale || type == LayerType::Transparent;
}

bool EncodeRasterResource(Layer* layer, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    if (!layer) return false;
    auto* codec = CoreServices::GetImageCodec();
    if (!codec) return false;

    const uint32_t width = layer->GetCanvasWidth();
    const uint32_t height = layer->GetCanvasHeight();
    std::vector<uint16_t> rgba16(static_cast<size_t>(width) * height * 4, 0);

    for (uint32_t gy = 0; gy < layer->GetGridHeight(); ++gy) {
        for (uint32_t gx = 0; gx < layer->GetGridWidth(); ++gx) {
            Render::Tile* tile = layer->GetTile(gx, gy);
            if (!tile) continue;
            for (uint32_t ly = 0; ly < Render::TILE_SIZE; ++ly) {
                const uint32_t y = gy * Render::TILE_SIZE + ly;
                if (y >= height) break;
                for (uint32_t lx = 0; lx < Render::TILE_SIZE; ++lx) {
                    const uint32_t x = gx * Render::TILE_SIZE + lx;
                    if (x >= width) break;
                    const size_t tileIdx = (static_cast<size_t>(ly) * Render::TILE_SIZE + lx) * Render::TILE_CHANNELS;
                    const size_t outIdx = (static_cast<size_t>(y) * width + x) * 4;
                    rgba16[outIdx] = Expand10To16(tile->data[tileIdx]);
                    rgba16[outIdx + 1] = Expand10To16(tile->data[tileIdx + 1]);
                    rgba16[outIdx + 2] = Expand10To16(tile->data[tileIdx + 2]);
                    rgba16[outIdx + 3] = Expand10To16(tile->data[tileIdx + 3]);
                }
            }
        }
    }

    return codec->EncodePngRgba16ToBytes(width, height, rgba16, outBytes);
}

std::vector<uint8_t> BuildFallbackSvgResource(Layer* layer) {
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << layer->GetCanvasWidth() << "\" height=\"" << layer->GetCanvasHeight() << "\" "
        << "cloverpic:layer=\"true\" xmlns:cloverpic=\"https://cloverpic.app/schema/v1\"";
    if (layer->GetType() == LayerType::Text) {
        svg << " cloverpic:text-layer=\"true\"";
    }
    svg << "></svg>";
    const std::string text = svg.str();
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::vector<uint8_t> BuildResourceChunkBytes(const ResourceBlob& resource) {
    std::ostringstream stream(std::ios::binary);
    WriteString(stream, resource.id);
    WriteU64(stream, resource.layerIndex);
    WriteU32(stream, static_cast<uint32_t>(resource.kind));
    WriteU32(stream, static_cast<uint32_t>(resource.pixelFormat));
    WriteU16(stream, resource.bitDepth);
    WriteU16(stream, 0);
    const auto& m = resource.metadata;
    WriteString(stream, m.name);
    WriteU32(stream, static_cast<uint32_t>(m.type));
    WriteU32(stream, static_cast<uint32_t>(m.blendMode));
    WriteU8(stream, m.opacity);
    WriteU8(stream, m.visible ? 1 : 0);
    WriteU8(stream, m.locked ? 1 : 0);
    WriteU8(stream, m.protectAlpha ? 1 : 0);
    WriteBytes(stream, resource.bytes);
    return StreamToBytes(stream);
}

bool ParseResourceChunkBytes(const std::vector<uint8_t>& bytes, ResourceBlob& outResource) {
    auto stream = BytesToStream(bytes);
    uint32_t kind = 0;
    uint32_t pixelFormat = 0;
    uint16_t bitDepth = 0;
    uint16_t reserved = 0;
    uint32_t type = 0;
    uint32_t blend = 0;
    uint8_t opacity = 255;
    uint8_t visible = 1;
    uint8_t locked = 0;
    uint8_t protect = 0;

    if (!ReadString(stream, outResource.id) || !ReadU64(stream, outResource.layerIndex) ||
        !ReadU32(stream, kind) || !ReadU32(stream, pixelFormat) ||
        !ReadU16(stream, bitDepth) || !ReadU16(stream, reserved) ||
        !ReadString(stream, outResource.metadata.name) ||
        !ReadU32(stream, type) || !ReadU32(stream, blend) ||
        !ReadU8(stream, opacity) || !ReadU8(stream, visible) ||
        !ReadU8(stream, locked) || !ReadU8(stream, protect) ||
        !ReadBytes(stream, outResource.bytes)) {
        return false;
    }

    outResource.kind = static_cast<ResourceKind>(kind);
    outResource.pixelFormat = static_cast<PixelFormat>(pixelFormat);
    outResource.bitDepth = bitDepth;
    outResource.metadata.layerIndex = outResource.layerIndex;
    outResource.metadata.type = static_cast<LayerType>(type);
    outResource.metadata.blendMode = static_cast<BlendMode>(blend);
    outResource.metadata.opacity = opacity;
    outResource.metadata.visible = visible != 0;
    outResource.metadata.locked = locked != 0;
    outResource.metadata.protectAlpha = protect != 0;
    outResource.metadata.resourceId = outResource.id;
    outResource.metadata.resourceKind = outResource.kind;
    outResource.metadata.pixelFormat = outResource.pixelFormat;
    outResource.metadata.bitDepth = outResource.bitDepth;
    outResource.metadata.resourceSize = static_cast<uint64_t>(outResource.bytes.size());
    return true;
}

std::vector<uint8_t> BuildManifest(Project* project,
                                   LayerManager* layerManager,
                                   const std::vector<ResourceBlob>& resources) {
    std::ostringstream stream(std::ios::binary);
    WriteString(stream, project ? project->GetName() : L"Untitled");
    const auto& canvas = project->GetCanvas();
    WriteU32(stream, canvas.widthPx);
    WriteU32(stream, canvas.heightPx);
    WriteFloat(stream, canvas.dpi);
    WriteU32(stream, static_cast<uint32_t>(canvas.colorMode));
    WriteU8(stream, canvas.transparent ? 1 : 0);
    WriteU32(stream, static_cast<uint32_t>(canvas.initialLayerType));
    WriteString(stream, canvas.rgbProfile);
    WriteString(stream, canvas.cmykProfile);
    WriteBytes(stream, canvas.rgbProfileBytes);
    WriteBytes(stream, canvas.cmykProfileBytes);
    WriteU64(stream, static_cast<uint64_t>(layerManager->GetActiveLayerIndex()));
    WriteU64(stream, static_cast<uint64_t>(resources.size()));

    for (const auto& resource : resources) {
        const auto& m = resource.metadata;
        WriteU64(stream, m.layerIndex);
        WriteString(stream, m.name);
        WriteU32(stream, static_cast<uint32_t>(m.type));
        WriteU32(stream, static_cast<uint32_t>(m.blendMode));
        WriteU8(stream, m.opacity);
        WriteU8(stream, m.visible ? 1 : 0);
        WriteU8(stream, m.locked ? 1 : 0);
        WriteU8(stream, m.protectAlpha ? 1 : 0);
        WriteString(stream, m.resourceId);
        WriteU32(stream, static_cast<uint32_t>(m.resourceKind));
        WriteU32(stream, static_cast<uint32_t>(m.pixelFormat));
        WriteU16(stream, m.bitDepth);
        WriteU64(stream, m.resourceSize);
    }
    return StreamToBytes(stream);
}

bool ParseManifest(const std::vector<uint8_t>& bytes,
                   Ref<Project>& project,
                   uint32_t& outCanvasWidth,
                   uint32_t& outCanvasHeight,
                   uint64_t& outActiveLayer,
                   std::vector<LayerManifestEntry>& outLayers) {
    auto stream = BytesToStream(bytes);
    String projectName;
    uint32_t width = 0;
    uint32_t height = 0;
    float dpi = 350.0f;
    uint32_t colorMode = 0;
    uint8_t transparent = 1;
    uint32_t initialLayerType = static_cast<uint32_t>(LayerType::Transparent);
    uint64_t activeLayer = 0;
    uint64_t layerCount = 0;

    if (!ReadString(stream, projectName) || !ReadU32(stream, width) || !ReadU32(stream, height) ||
        !ReadFloat(stream, dpi) || !ReadU32(stream, colorMode) || !ReadU8(stream, transparent) ||
        !ReadU32(stream, initialLayerType)) {
        return false;
    }

    project = MakeRef<Project>(projectName.empty() ? L"Untitled" : projectName);
    auto& canvas = project->GetCanvas();
    canvas.widthPx = width;
    canvas.heightPx = height;
    canvas.dpi = dpi;
    canvas.colorMode = static_cast<ColorMode>(colorMode);
    canvas.transparent = transparent != 0;
    canvas.initialLayerType = static_cast<LayerType>(initialLayerType);
    if (!ReadString(stream, canvas.rgbProfile) || !ReadString(stream, canvas.cmykProfile) ||
        !ReadBytes(stream, canvas.rgbProfileBytes) || !ReadBytes(stream, canvas.cmykProfileBytes) ||
        !ReadU64(stream, activeLayer) || !ReadU64(stream, layerCount)) {
        return false;
    }

    outLayers.clear();
    outLayers.reserve(static_cast<size_t>(std::min<uint64_t>(layerCount, 1'000'000)));
    for (uint64_t i = 0; i < layerCount; ++i) {
        LayerManifestEntry m;
        uint32_t type = 0;
        uint32_t blend = 0;
        uint8_t visible = 1;
        uint8_t locked = 0;
        uint8_t protect = 0;
        uint32_t kind = 0;
        uint32_t pixelFormat = 0;
        if (!ReadU64(stream, m.layerIndex) || !ReadString(stream, m.name) ||
            !ReadU32(stream, type) || !ReadU32(stream, blend) ||
            !ReadU8(stream, m.opacity) || !ReadU8(stream, visible) ||
            !ReadU8(stream, locked) || !ReadU8(stream, protect) ||
            !ReadString(stream, m.resourceId) || !ReadU32(stream, kind) ||
            !ReadU32(stream, pixelFormat) || !ReadU16(stream, m.bitDepth) ||
            !ReadU64(stream, m.resourceSize)) {
            return false;
        }
        m.type = static_cast<LayerType>(type);
        m.blendMode = static_cast<BlendMode>(blend);
        m.visible = visible != 0;
        m.locked = locked != 0;
        m.protectAlpha = protect != 0;
        m.resourceKind = static_cast<ResourceKind>(kind);
        m.pixelFormat = static_cast<PixelFormat>(pixelFormat);
        outLayers.push_back(std::move(m));
    }

    outCanvasWidth = width;
    outCanvasHeight = height;
    outActiveLayer = activeLayer;
    return width > 0 && height > 0;
}

std::vector<uint8_t> BuildIndex(const std::vector<ResourceBlob>& resources) {
    std::ostringstream stream(std::ios::binary);
    WriteU64(stream, static_cast<uint64_t>(resources.size()));
    for (const auto& resource : resources) {
        WriteString(stream, resource.id);
        WriteU64(stream, resource.layerIndex);
        WriteU32(stream, static_cast<uint32_t>(resource.kind));
        WriteU64(stream, resource.chunkOffset);
        WriteU64(stream, static_cast<uint64_t>(resource.bytes.size()));
        WriteU32(stream, resource.crc);
    }
    return StreamToBytes(stream);
}

void WriteChunk(std::ostream& stream, const char type[8], const std::vector<uint8_t>& bytes) {
    stream.write(type, 8);
    WriteU64(stream, static_cast<uint64_t>(bytes.size()));
    WriteU32(stream, Crc32Like(bytes));
    WriteU32(stream, 0);
    WriteRaw(stream, bytes.data(), bytes.size());
}

} // namespace

bool ProjectSerializer::SaveProject(const String& filePath, Project* project, LayerManager* layerManager) {
    if (!project || !layerManager) return false;
    auto* fileSystem = CoreServices::GetFileSystem();
    if (!fileSystem) return false;

    std::vector<ResourceBlob> resources;
    resources.reserve(layerManager->GetLayerCount());
    for (uint64_t i = 0; i < static_cast<uint64_t>(layerManager->GetLayerCount()); ++i) {
        auto layer = layerManager->GetLayer(static_cast<size_t>(i));
        if (!layer) return false;

        ResourceBlob resource;
        resource.id = L"layer-" + std::to_wstring(i);
        resource.layerIndex = i;
        resource.metadata.layerIndex = i;
        resource.metadata.name = layer->GetName();
        resource.metadata.type = layer->GetType();
        resource.metadata.blendMode = layer->GetBlendMode();
        resource.metadata.opacity = layer->GetOpacity();
        resource.metadata.visible = layer->IsVisible();
        resource.metadata.locked = layer->IsLocked();
        resource.metadata.protectAlpha = layer->IsProtectAlpha();
        resource.metadata.resourceId = resource.id;

        if (IsRasterLayerType(layer->GetType())) {
            resource.kind = ResourceKind::RasterPng16;
            resource.pixelFormat = PixelFormat::Rgba10Unorm;
            resource.bitDepth = 10;
            if (!EncodeRasterResource(layer.get(), resource.bytes)) {
                return false;
            }
        } else {
            resource.kind = layer->GetType() == LayerType::Text ? ResourceKind::Svg : ResourceKind::Payload;
            resource.pixelFormat = PixelFormat::Rgba10Unorm;
            resource.bitDepth = 10;
            resource.bytes = layer->SerializePayload();
            if (resource.bytes.empty()) {
                resource.bytes = BuildFallbackSvgResource(layer.get());
            }
        }

        resource.metadata.resourceKind = resource.kind;
        resource.metadata.pixelFormat = resource.pixelFormat;
        resource.metadata.bitDepth = resource.bitDepth;
        resource.metadata.resourceSize = static_cast<uint64_t>(resource.bytes.size());
        resource.crc = Crc32Like(resource.bytes);
        resources.push_back(std::move(resource));
    }

    const auto manifest = BuildManifest(project, layerManager, resources);
    const char manifestType[8] = {'M', 'A', 'N', 'I', 'F', 'E', 'S', 'T'};
    const char indexType[8] = {'I', 'N', 'D', 'E', 'X', 0, 0, 0};
    const char resourceType[8] = {'R', 'E', 'S', 'O', 'U', 'R', 'C', 'E'};

    std::vector<std::vector<uint8_t>> resourceChunks;
    resourceChunks.reserve(resources.size());
    for (const auto& resource : resources) {
        resourceChunks.push_back(BuildResourceChunkBytes(resource));
    }

    std::vector<uint8_t> provisionalIndex = BuildIndex(resources);
    uint64_t offset = HeaderBytes;
    offset += 24 + manifest.size();
    offset += 24 + provisionalIndex.size();
    for (size_t i = 0; i < resources.size(); ++i) {
        resources[i].chunkOffset = offset + 24;
        offset += 24 + resourceChunks[i].size();
    }
    const auto index = BuildIndex(resources);

    std::ostringstream stream(std::ios::binary);
    stream.write(CloverMagic, sizeof(CloverMagic));
    WriteU16(stream, CloverVersion);
    WriteU16(stream, 0);
    WriteU32(stream, HeaderBytes);
    WriteU64(stream, static_cast<uint64_t>(2 + resources.size()));
    WriteChunk(stream, manifestType, manifest);
    WriteChunk(stream, indexType, index);
    for (const auto& chunk : resourceChunks) {
        WriteChunk(stream, resourceType, chunk);
    }

    if (!stream.good()) return false;
    return fileSystem->WriteFileBytes(filePath, StreamToBytes(stream));
}

Ref<Project> ProjectSerializer::LoadProject(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return nullptr;
    auto* fileSystem = CoreServices::GetFileSystem();
    auto* codec = CoreServices::GetImageCodec();
    if (!fileSystem || !codec) return nullptr;

    std::vector<uint8_t> bytes;
    if (!fileSystem->ReadFileBytes(filePath, bytes) || bytes.size() < HeaderBytes) return nullptr;
    auto stream = BytesToStream(bytes);

    char magic[8] = {};
    uint16_t version = 0;
    uint16_t flags = 0;
    uint32_t headerBytes = 0;
    uint64_t chunkCount = 0;
    if (!ReadRaw(stream, magic, sizeof(magic)) || std::memcmp(magic, CloverMagic, sizeof(CloverMagic)) != 0 ||
        !ReadU16(stream, version) || version != CloverVersion ||
        !ReadU16(stream, flags) || !ReadU32(stream, headerBytes) ||
        headerBytes != HeaderBytes || !ReadU64(stream, chunkCount)) {
        return nullptr;
    }

    std::vector<uint8_t> manifestBytes;
    std::vector<ResourceBlob> resources;
    for (uint64_t i = 0; i < chunkCount; ++i) {
        char type[8] = {};
        uint64_t size = 0;
        uint32_t crc = 0;
        uint32_t reserved = 0;
        if (!ReadRaw(stream, type, sizeof(type)) || !ReadU64(stream, size) ||
            !ReadU32(stream, crc) || !ReadU32(stream, reserved) || size > (1ull << 34)) {
            return nullptr;
        }
        std::vector<uint8_t> chunk(static_cast<size_t>(size));
        if (!ReadRaw(stream, chunk.data(), chunk.size())) return nullptr;
        if (Crc32Like(chunk) != crc) return nullptr;
        if (std::memcmp(type, "MANIFEST", 8) == 0) {
            manifestBytes = std::move(chunk);
        } else if (std::memcmp(type, "RESOURCE", 8) == 0) {
            ResourceBlob resource;
            if (ParseResourceChunkBytes(chunk, resource)) {
                resources.push_back(std::move(resource));
            }
        }
    }

    Ref<Project> project;
    uint32_t canvasW = 0;
    uint32_t canvasH = 0;
    uint64_t activeLayer = 0;
    std::vector<LayerManifestEntry> manifestLayers;
    if (manifestBytes.empty() || !ParseManifest(manifestBytes, project, canvasW, canvasH, activeLayer, manifestLayers)) {
        return nullptr;
    }

    std::sort(manifestLayers.begin(), manifestLayers.end(), [](const auto& a, const auto& b) {
        return a.layerIndex < b.layerIndex;
    });

    layerManager->Initialize(canvasW, canvasH);
    for (const auto& entry : manifestLayers) {
        auto found = std::find_if(resources.begin(), resources.end(), [&](const ResourceBlob& resource) {
            return resource.id == entry.resourceId || resource.layerIndex == entry.layerIndex;
        });
        if (found == resources.end()) return nullptr;

        auto layer = CreateLayerFromType(entry.name.empty() ? L"Layer" : entry.name, entry.type, canvasW, canvasH);
        layer->SetBlendMode(entry.blendMode);
        layer->SetOpacity(entry.opacity);
        layer->SetVisible(entry.visible);
        layer->SetLocked(false);
        layer->SetProtectAlpha(entry.protectAlpha);

        if (found->kind == ResourceKind::RasterPng16) {
            uint32_t pngW = 0;
            uint32_t pngH = 0;
            std::vector<uint16_t> rgba16;
            if (!codec->DecodePngRgba16FromBytes(found->bytes, pngW, pngH, rgba16) ||
                pngW != canvasW || pngH != canvasH ||
                rgba16.size() < static_cast<size_t>(canvasW) * canvasH * 4) {
                return nullptr;
            }
            auto raster = std::dynamic_pointer_cast<RasterLayer>(layer);
            if (!raster) return nullptr;
            for (uint32_t y = 0; y < canvasH; ++y) {
                for (uint32_t x = 0; x < canvasW; ++x) {
                    const size_t idx = (static_cast<size_t>(y) * canvasW + x) * 4;
                    Color10 pixel(Collapse16To10(rgba16[idx]),
                                  Collapse16To10(rgba16[idx + 1]),
                                  Collapse16To10(rgba16[idx + 2]),
                                  Collapse16To10(rgba16[idx + 3]));
                    if (pixel.r || pixel.g || pixel.b || pixel.a) {
                        raster->SetPixel10(x, y, pixel);
                    }
                }
            }
        } else {
            layer->DeserializePayload(found->bytes.data(), found->bytes.size());
            layer->RasterizeIfNeeded();
        }

        layer->SetLocked(entry.locked);
        layer->SetDirty(true);
        layerManager->AddLayer(layer);
    }

    if (activeLayer < static_cast<uint64_t>(layerManager->GetLayerCount())) {
        layerManager->SetActiveLayer(static_cast<size_t>(activeLayer));
    }
    layerManager->MarkCompositeDirty();
    return project;
}

bool ProjectSerializer::ExportPNG(const String& filePath, LayerManager* layerManager) {
    if (!layerManager) return false;
    auto* codec = CoreServices::GetImageCodec();
    if (!codec) return false;

    const uint32_t width = layerManager->GetCanvasWidth();
    const uint32_t height = layerManager->GetCanvasHeight();
    if (width == 0 || height == 0) return false;

    std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * 4);
    layerManager->CompositeToBuffer(buffer.data(), width, height);
    return codec->EncodeDisplayPngBgra8ToFile(filePath, width, height, buffer);
}

bool ProjectSerializer::WriteLayer(std::ostream&, Layer*) {
    return false;
}

Ref<Layer> ProjectSerializer::ReadLayer(std::istream&, uint32_t, uint32_t) {
    return nullptr;
}

} // namespace CloverPic
