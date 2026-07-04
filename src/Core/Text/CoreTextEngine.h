#pragma once

#include "Core/Services/CoreServices.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace CloverPic::CoreText {

struct FontFaceInfo {
    size_t id = 0;
    String family;
    String style;
    String fullName;
    String path;
    uint32_t faceOffset = 0;
};

class CoreTextEngine {
public:
    struct ParsedFont;
    struct GlyphOutline;
    struct GlyphBitmap;

    static CoreTextEngine& Get();

    bool Initialize(const PlatformFontCatalog& platformCatalog);
    bool IsInitialized() const { return m_initialized; }
    const String& GetSystemUiFamily() const { return m_systemUiFamily; }

    bool RasterizeText(const TextRasterRequest& request, RasterizedTextBitmap& outBitmap);

private:
    CoreTextEngine() = default;

    bool ProbeFontFile(const String& path, std::vector<FontFaceInfo>& outFaces);
    ParsedFont* LoadFace(size_t faceId);
    size_t FindFaceForFamily(const String& family) const;
    size_t FindFallbackFace(uint32_t codepoint, size_t preferredFace);
    bool LoadGlyph(size_t faceId, uint32_t codepoint, float fontSizePx, GlyphBitmap& outGlyph);
    bool RasterizeGlyph(const ParsedFont& font, const GlyphOutline& outline, float fontSizePx, GlyphBitmap& outGlyph) const;
    std::vector<uint32_t> DecodeUtf16(const String& text) const;
    String NormalizeFamily(String value) const;

    bool m_initialized = false;
    String m_systemUiFamily = L"Segoe UI";
    std::vector<FontFaceInfo> m_faces;
    std::unordered_map<String, size_t> m_familyToFace;
    std::unordered_map<size_t, std::unique_ptr<ParsedFont>> m_loadedFaces;
};

} // namespace CloverPic::CoreText
