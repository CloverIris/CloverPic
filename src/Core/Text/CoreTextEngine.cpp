#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cwctype>
#include <limits>
#include <map>
#include <set>

namespace CloverPic::CoreText {

namespace {

constexpr size_t MissingFace = static_cast<size_t>(-1);
constexpr uint32_t Tag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

struct FontReader {
    const std::vector<uint8_t>* data = nullptr;
    uint32_t base = 0;

    bool CanRead(uint32_t offset, uint32_t size) const {
        return data && offset <= data->size() && size <= data->size() - offset;
    }
    uint8_t U8(uint32_t offset) const { return CanRead(offset, 1) ? (*data)[offset] : 0; }
    uint16_t U16(uint32_t offset) const {
        if (!CanRead(offset, 2)) return 0;
        return static_cast<uint16_t>(((*data)[offset] << 8) | (*data)[offset + 1]);
    }
    int16_t I16(uint32_t offset) const { return static_cast<int16_t>(U16(offset)); }
    uint32_t U32(uint32_t offset) const {
        if (!CanRead(offset, 4)) return 0;
        return (static_cast<uint32_t>((*data)[offset]) << 24) |
               (static_cast<uint32_t>((*data)[offset + 1]) << 16) |
               (static_cast<uint32_t>((*data)[offset + 2]) << 8) |
               static_cast<uint32_t>((*data)[offset + 3]);
    }
};

bool ReadFileBytes(const String& path, std::vector<uint8_t>& bytes) {
    auto* fileSystem = CoreServices::GetFileSystem();
    return fileSystem && fileSystem->ReadFileBytes(path, bytes) && !bytes.empty();
}

String DecodeUtf16Be(const std::vector<uint8_t>& data, uint32_t offset, uint32_t byteLength) {
    String result;
    for (uint32_t i = 0; i + 1 < byteLength; i += 2) {
        const uint16_t ch = static_cast<uint16_t>((data[offset + i] << 8) | data[offset + i + 1]);
        if (ch != 0) result.push_back(static_cast<wchar_t>(ch));
    }
    return result;
}

String DecodeAscii(const std::vector<uint8_t>& data, uint32_t offset, uint32_t byteLength) {
    String result;
    for (uint32_t i = 0; i < byteLength; ++i) {
        const uint8_t ch = data[offset + i];
        if (ch >= 32 && ch < 127) result.push_back(static_cast<wchar_t>(ch));
    }
    return result;
}

struct Vec2 {
    float x = 0;
    float y = 0;
};

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Vec2 Quad(const Vec2& a, const Vec2& b, const Vec2& c, float t) {
    return Vec2{Lerp(Lerp(a.x, b.x, t), Lerp(b.x, c.x, t), t),
                Lerp(Lerp(a.y, b.y, t), Lerp(b.y, c.y, t), t)};
}

} // namespace

struct CoreTextEngine::GlyphOutline {
    std::vector<std::vector<Vec2>> contours;
    int16_t xMin = 0;
    int16_t yMin = 0;
    int16_t xMax = 0;
    int16_t yMax = 0;
    int advanceWidth = 0;
};

struct CoreTextEngine::GlyphBitmap {
    uint32_t width = 0;
    uint32_t height = 0;
    int32_t bearingX = 0;
    int32_t bearingY = 0;
    int32_t advanceX = 0;
    std::vector<uint8_t> alpha;
};

struct CoreTextEngine::ParsedFont {
    std::vector<uint8_t> data;
    FontReader reader;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> tables;
    String family;
    String style;
    uint16_t unitsPerEm = 1000;
    int16_t ascent = 800;
    int16_t descent = -200;
    int16_t lineGap = 0;
    int16_t indexToLocFormat = 0;
    uint16_t numGlyphs = 0;
    uint16_t numHMetrics = 0;
    std::vector<uint32_t> glyphOffsets;
    std::vector<uint16_t> advanceWidths;
    std::vector<int16_t> leftSideBearings;
    std::map<uint64_t, int16_t> kerning;
    std::map<uint64_t, GlyphBitmap> glyphCache;

    bool Has(uint32_t tag) const { return tables.find(tag) != tables.end(); }
    uint32_t Table(uint32_t tag) const {
        const auto it = tables.find(tag);
        return it == tables.end() ? 0 : it->second.first;
    }
    uint32_t TableLen(uint32_t tag) const {
        const auto it = tables.find(tag);
        return it == tables.end() ? 0 : it->second.second;
    }
};

namespace {

std::vector<uint32_t> FaceOffsets(const std::vector<uint8_t>& data) {
    FontReader r{&data, 0};
    std::vector<uint32_t> offsets;
    const uint32_t signature = r.U32(0);
    if (signature == Tag('t', 't', 'c', 'f')) {
        const uint32_t count = r.U32(8);
        for (uint32_t i = 0; i < count && r.CanRead(12 + i * 4, 4); ++i) {
            offsets.push_back(r.U32(12 + i * 4));
        }
    } else {
        offsets.push_back(0);
    }
    return offsets;
}

bool ParseTables(CoreTextEngine::ParsedFont& font, uint32_t faceOffset) {
    auto& r = font.reader;
    r.data = &font.data;
    r.base = faceOffset;
    const uint32_t sfnt = r.U32(faceOffset);
    if (sfnt != 0x00010000 && sfnt != Tag('t', 'r', 'u', 'e')) {
        return false;
    }
    const uint16_t numTables = r.U16(faceOffset + 4);
    for (uint16_t i = 0; i < numTables; ++i) {
        const uint32_t rec = faceOffset + 12 + i * 16;
        const uint32_t tag = r.U32(rec);
        const uint32_t offset = r.U32(rec + 8);
        const uint32_t length = r.U32(rec + 12);
        if (r.CanRead(offset, length)) {
            font.tables[tag] = {offset, length};
        }
    }
    return font.Has(Tag('h','e','a','d')) && font.Has(Tag('m','a','x','p')) &&
           font.Has(Tag('h','h','e','a')) && font.Has(Tag('h','m','t','x')) &&
           font.Has(Tag('l','o','c','a')) && font.Has(Tag('g','l','y','f')) &&
           font.Has(Tag('c','m','a','p'));
}

void ParseNames(CoreTextEngine::ParsedFont& font) {
    const uint32_t name = font.Table(Tag('n','a','m','e'));
    const uint32_t len = font.TableLen(Tag('n','a','m','e'));
    if (!name || len < 6) return;
    auto& r = font.reader;
    const uint16_t count = r.U16(name + 2);
    const uint16_t stringOffset = r.U16(name + 4);
    for (uint16_t i = 0; i < count; ++i) {
        const uint32_t rec = name + 6 + i * 12;
        if (!r.CanRead(rec, 12)) break;
        const uint16_t platform = r.U16(rec);
        const uint16_t nameId = r.U16(rec + 6);
        const uint16_t byteLen = r.U16(rec + 8);
        const uint16_t offset = r.U16(rec + 10);
        const uint32_t str = name + stringOffset + offset;
        if (!r.CanRead(str, byteLen)) continue;
        String value = (platform == 3) ? DecodeUtf16Be(font.data, str, byteLen) : DecodeAscii(font.data, str, byteLen);
        if (value.empty()) continue;
        if (nameId == 1 && font.family.empty()) font.family = value;
        if (nameId == 2 && font.style.empty()) font.style = value;
    }
}

void ParseMetrics(CoreTextEngine::ParsedFont& font) {
    auto& r = font.reader;
    const uint32_t head = font.Table(Tag('h','e','a','d'));
    const uint32_t maxp = font.Table(Tag('m','a','x','p'));
    const uint32_t hhea = font.Table(Tag('h','h','e','a'));
    font.unitsPerEm = std::max<uint16_t>(1, r.U16(head + 18));
    font.indexToLocFormat = r.I16(head + 50);
    font.numGlyphs = r.U16(maxp + 4);
    font.ascent = r.I16(hhea + 4);
    font.descent = r.I16(hhea + 6);
    font.lineGap = r.I16(hhea + 8);
    font.numHMetrics = r.U16(hhea + 34);

    const uint32_t hmtx = font.Table(Tag('h','m','t','x'));
    uint16_t lastAdvance = 0;
    for (uint16_t i = 0; i < font.numGlyphs; ++i) {
        uint16_t advance = lastAdvance;
        int16_t bearing = 0;
        if (i < font.numHMetrics) {
            advance = r.U16(hmtx + i * 4);
            bearing = r.I16(hmtx + i * 4 + 2);
            lastAdvance = advance;
        } else {
            const uint32_t lsbOffset = hmtx + font.numHMetrics * 4 + (i - font.numHMetrics) * 2;
            bearing = r.I16(lsbOffset);
        }
        font.advanceWidths.push_back(advance);
        font.leftSideBearings.push_back(bearing);
    }

    const uint32_t loca = font.Table(Tag('l','o','c','a'));
    for (uint32_t i = 0; i <= font.numGlyphs; ++i) {
        font.glyphOffsets.push_back(font.indexToLocFormat == 0 ? r.U16(loca + i * 2) * 2u : r.U32(loca + i * 4));
    }
}

void ParseKerning(CoreTextEngine::ParsedFont& font) {
    const uint32_t kern = font.Table(Tag('k','e','r','n'));
    if (!kern) return;
    auto& r = font.reader;
    uint32_t pos = kern + 4;
    const uint16_t nTables = r.U16(kern + 2);
    for (uint16_t t = 0; t < nTables; ++t) {
        const uint32_t sub = pos;
        const uint16_t length = r.U16(sub + 2);
        const uint16_t coverage = r.U16(sub + 4);
        const uint16_t format = coverage >> 8;
        if (format == 0 && length >= 14) {
            const uint16_t nPairs = r.U16(sub + 6);
            for (uint16_t i = 0; i < nPairs; ++i) {
                const uint32_t pair = sub + 14 + i * 6;
                const uint16_t left = r.U16(pair);
                const uint16_t right = r.U16(pair + 2);
                const int16_t value = r.I16(pair + 4);
                font.kerning[(static_cast<uint64_t>(left) << 32) | right] = value;
            }
        }
        if (length == 0) break;
        pos += length;
    }
}

uint32_t LookupGlyph(const CoreTextEngine::ParsedFont& font, uint32_t codepoint) {
    const uint32_t cmap = font.Table(Tag('c','m','a','p'));
    auto& r = font.reader;
    const uint16_t numTables = r.U16(cmap + 2);
    uint32_t best12 = 0;
    uint32_t best4 = 0;
    for (uint16_t i = 0; i < numTables; ++i) {
        const uint32_t rec = cmap + 4 + i * 8;
        const uint16_t platform = r.U16(rec);
        const uint16_t encoding = r.U16(rec + 2);
        const uint32_t offset = cmap + r.U32(rec + 4);
        const uint16_t format = r.U16(offset);
        if (format == 12 && platform == 3 && encoding == 10) best12 = offset;
        if (format == 4 && platform == 3 && (encoding == 1 || encoding == 0)) best4 = offset;
    }
    if (best12) {
        const uint32_t groups = r.U32(best12 + 12);
        for (uint32_t i = 0; i < groups; ++i) {
            const uint32_t g = best12 + 16 + i * 12;
            const uint32_t start = r.U32(g);
            const uint32_t end = r.U32(g + 4);
            const uint32_t startGlyph = r.U32(g + 8);
            if (codepoint >= start && codepoint <= end) return startGlyph + codepoint - start;
        }
    }
    if (best4 && codepoint <= 0xFFFF) {
        const uint16_t segCount = r.U16(best4 + 6) / 2;
        const uint32_t endCodes = best4 + 14;
        const uint32_t startCodes = endCodes + segCount * 2 + 2;
        const uint32_t deltas = startCodes + segCount * 2;
        const uint32_t ranges = deltas + segCount * 2;
        for (uint16_t i = 0; i < segCount; ++i) {
            const uint16_t end = r.U16(endCodes + i * 2);
            const uint16_t start = r.U16(startCodes + i * 2);
            if (codepoint < start || codepoint > end) continue;
            const int16_t delta = r.I16(deltas + i * 2);
            const uint16_t range = r.U16(ranges + i * 2);
            if (range == 0) return static_cast<uint16_t>(codepoint + delta);
            const uint32_t glyphOffset = ranges + i * 2 + range + (codepoint - start) * 2;
            const uint16_t glyph = r.U16(glyphOffset);
            return glyph == 0 ? 0 : static_cast<uint16_t>(glyph + delta);
        }
    }
    return 0;
}

void FlattenContour(const std::vector<Vec2>& raw, const std::vector<bool>& onCurve, std::vector<Vec2>& out) {
    if (raw.empty()) return;
    std::vector<Vec2> points;
    std::vector<bool> on;
    for (size_t i = 0; i < raw.size(); ++i) {
        points.push_back(raw[i]);
        on.push_back(onCurve[i]);
        const size_t j = (i + 1) % raw.size();
        if (!onCurve[i] && !onCurve[j]) {
            points.push_back(Vec2{(raw[i].x + raw[j].x) * 0.5f, (raw[i].y + raw[j].y) * 0.5f});
            on.push_back(true);
        }
    }
    size_t start = 0;
    while (start < on.size() && !on[start]) ++start;
    if (start == on.size()) return;
    Vec2 current = points[start];
    out.push_back(current);
    for (size_t step = 1; step <= points.size(); ++step) {
        const size_t i = (start + step) % points.size();
        if (on[i]) {
            current = points[i];
            out.push_back(current);
        } else {
            const size_t next = (i + 1) % points.size();
            const Vec2 control = points[i];
            const Vec2 end = points[next];
            for (int s = 1; s <= 8; ++s) {
                out.push_back(Quad(current, control, end, s / 8.0f));
            }
            current = end;
            ++step;
        }
    }
}

bool PointInside(const std::vector<std::vector<Vec2>>& contours, float x, float y) {
    bool inside = false;
    for (const auto& c : contours) {
        if (c.size() < 3) continue;
        for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) {
            const bool crosses = ((c[i].y > y) != (c[j].y > y)) &&
                (x < (c[j].x - c[i].x) * (y - c[i].y) / ((c[j].y - c[i].y) == 0 ? 1.0f : (c[j].y - c[i].y)) + c[i].x);
            if (crosses) inside = !inside;
        }
    }
    return inside;
}

} // namespace

CoreTextEngine& CoreTextEngine::Get() {
    static CoreTextEngine engine;
    return engine;
}

bool CoreTextEngine::Initialize(const PlatformFontCatalog& platformCatalog) {
    if (m_initialized) return true;
    m_systemUiFamily = platformCatalog.systemUiFamily.empty() ? L"Segoe UI" : platformCatalog.systemUiFamily;
    for (const auto& font : platformCatalog.fonts) {
        ProbeFontFile(font.path, m_faces);
    }
    for (const auto& face : m_faces) {
        const String normalized = NormalizeFamily(face.family);
        if (!normalized.empty() && m_familyToFace.find(normalized) == m_familyToFace.end()) {
            m_familyToFace[normalized] = face.id;
        }
    }
    m_initialized = !m_faces.empty();
    return m_initialized;
}

bool CoreTextEngine::ProbeFontFile(const String& path, std::vector<FontFaceInfo>& outFaces) {
    std::vector<uint8_t> data;
    if (!ReadFileBytes(path, data)) return false;
    for (uint32_t offset : FaceOffsets(data)) {
        auto font = std::make_unique<ParsedFont>();
        font->data = data;
        if (!ParseTables(*font, offset)) continue;
        ParseNames(*font);
        if (font->family.empty()) continue;
        FontFaceInfo info;
        info.id = outFaces.size();
        info.family = font->family;
        info.style = font->style;
        info.fullName = info.family + (info.style.empty() ? L"" : L" " + info.style);
        info.path = path;
        info.faceOffset = offset;
        outFaces.push_back(std::move(info));
    }
    return true;
}

CoreTextEngine::ParsedFont* CoreTextEngine::LoadFace(size_t faceId) {
    if (faceId >= m_faces.size()) return nullptr;
    auto found = m_loadedFaces.find(faceId);
    if (found != m_loadedFaces.end()) return found->second.get();
    std::vector<uint8_t> data;
    if (!ReadFileBytes(m_faces[faceId].path, data)) return nullptr;
    auto font = std::make_unique<ParsedFont>();
    font->data = std::move(data);
    if (!ParseTables(*font, m_faces[faceId].faceOffset)) return nullptr;
    ParseNames(*font);
    ParseMetrics(*font);
    ParseKerning(*font);
    auto* result = font.get();
    m_loadedFaces[faceId] = std::move(font);
    return result;
}

bool CoreTextEngine::RasterizeText(const TextRasterRequest& request, RasterizedTextBitmap& outBitmap) {
    outBitmap = {};
    if (!m_initialized || request.text.empty()) return false;
    const auto codepoints = DecodeUtf16(request.text);
    if (codepoints.empty()) return true;

    const size_t preferred = FindFaceForFamily(request.fontFamily.empty() ? m_systemUiFamily : request.fontFamily);
    const size_t defaultFace = preferred == MissingFace ? 0 : preferred;
    ParsedFont* metricsFont = LoadFace(defaultFace);
    if (!metricsFont) return false;
    const float scale = std::max(1.0f, request.fontSize) / metricsFont->unitsPerEm;
    const int baseline = static_cast<int>(std::ceil(metricsFont->ascent * scale));
    const int lineHeight = static_cast<int>(std::ceil((metricsFont->ascent - metricsFont->descent + metricsFont->lineGap) * scale));

    struct Placed { GlyphBitmap glyph; int x = 0; int y = 0; };
    std::vector<Placed> placed;
    int penX = 0;
    int maxRight = 1;
    int maxBottom = std::max(1, lineHeight);
    size_t prevFace = MissingFace;
    uint32_t prevGlyph = 0;

    for (uint32_t cp : codepoints) {
        if (cp == L'\n') continue;
        const size_t faceId = FindFallbackFace(cp, defaultFace);
        if (faceId == MissingFace) {
            penX += static_cast<int>(request.fontSize * 0.5f);
            continue;
        }
        ParsedFont* font = LoadFace(faceId);
        const uint32_t glyphId = font ? LookupGlyph(*font, cp) : 0;
        if (font && prevFace == faceId && prevGlyph != 0) {
            auto kern = font->kerning.find((static_cast<uint64_t>(prevGlyph) << 32) | glyphId);
            if (kern != font->kerning.end()) penX += static_cast<int>(kern->second * (request.fontSize / font->unitsPerEm));
        }
        GlyphBitmap glyph;
        if (!LoadGlyph(faceId, cp, request.fontSize, glyph)) continue;
        placed.push_back({glyph, penX + glyph.bearingX, baseline - glyph.bearingY});
        maxRight = std::max(maxRight, penX + glyph.advanceX + 2);
        maxBottom = std::max(maxBottom, placed.back().y + static_cast<int>(glyph.height) + 2);
        penX += glyph.advanceX;
        prevFace = faceId;
        prevGlyph = glyphId;
    }

    outBitmap.width = request.maxWidth == 0 ? static_cast<uint32_t>(maxRight) : std::min<uint32_t>(request.maxWidth, maxRight);
    outBitmap.height = request.maxHeight == 0 ? static_cast<uint32_t>(maxBottom) : std::min<uint32_t>(request.maxHeight, maxBottom);
    outBitmap.baseline = baseline;
    outBitmap.advanceX = penX;
    outBitmap.rgbaPixels.assign(static_cast<size_t>(outBitmap.width) * outBitmap.height * 4, 0);

    for (const auto& p : placed) {
        for (uint32_t y = 0; y < p.glyph.height; ++y) {
            const int dstY = p.y + static_cast<int>(y);
            if (dstY < 0 || dstY >= static_cast<int>(outBitmap.height)) continue;
            for (uint32_t x = 0; x < p.glyph.width; ++x) {
                const int dstX = p.x + static_cast<int>(x);
                if (dstX < 0 || dstX >= static_cast<int>(outBitmap.width)) continue;
                const uint8_t coverage = p.glyph.alpha[static_cast<size_t>(y) * p.glyph.width + x];
                if (coverage == 0) continue;
                const size_t idx = (static_cast<size_t>(dstY) * outBitmap.width + static_cast<size_t>(dstX)) * 4;
                outBitmap.rgbaPixels[idx + 0] = request.color.r;
                outBitmap.rgbaPixels[idx + 1] = request.color.g;
                outBitmap.rgbaPixels[idx + 2] = request.color.b;
                outBitmap.rgbaPixels[idx + 3] = static_cast<uint8_t>(static_cast<uint16_t>(coverage) * request.color.a / 255);
            }
        }
    }
    return true;
}

size_t CoreTextEngine::FindFaceForFamily(const String& family) const {
    auto it = m_familyToFace.find(NormalizeFamily(family));
    return it == m_familyToFace.end() ? MissingFace : it->second;
}

size_t CoreTextEngine::FindFallbackFace(uint32_t codepoint, size_t preferredFace) {
    if (preferredFace != MissingFace) {
        if (ParsedFont* font = LoadFace(preferredFace); font && LookupGlyph(*font, codepoint) != 0) return preferredFace;
    }
    for (const auto& face : m_faces) {
        if (ParsedFont* font = LoadFace(face.id); font && LookupGlyph(*font, codepoint) != 0) return face.id;
    }
    return MissingFace;
}

bool CoreTextEngine::LoadGlyph(size_t faceId, uint32_t codepoint, float fontSizePx, GlyphBitmap& outGlyph) {
    ParsedFont* font = LoadFace(faceId);
    if (!font) return false;
    const uint32_t glyphId = LookupGlyph(*font, codepoint);
    if (glyphId == 0 || glyphId >= font->numGlyphs) return false;
    const uint64_t cacheKey = (static_cast<uint64_t>(static_cast<uint32_t>(std::round(fontSizePx * 64))) << 32) | glyphId;
    auto cached = font->glyphCache.find(cacheKey);
    if (cached != font->glyphCache.end()) {
        outGlyph = cached->second;
        return true;
    }

    GlyphOutline outline;
    std::set<uint32_t> seen;
    auto loadOutline = [&](auto&& self, uint32_t gid, float tx, float ty, float a, float b, float c, float d) -> bool {
        if (gid >= font->numGlyphs || seen.count(gid)) return false;
        seen.insert(gid);
        const uint32_t glyf = font->Table(Tag('g','l','y','f'));
        const uint32_t start = glyf + font->glyphOffsets[gid];
        const uint32_t end = glyf + font->glyphOffsets[gid + 1];
        if (start >= end || !font->reader.CanRead(start, end - start)) return true;
        const int16_t contours = font->reader.I16(start);
        const int16_t xMin = font->reader.I16(start + 2);
        const int16_t yMin = font->reader.I16(start + 4);
        const int16_t xMax = font->reader.I16(start + 6);
        const int16_t yMax = font->reader.I16(start + 8);
        outline.xMin = std::min(outline.xMin, xMin);
        outline.yMin = std::min(outline.yMin, yMin);
        outline.xMax = std::max(outline.xMax, xMax);
        outline.yMax = std::max(outline.yMax, yMax);
        if (contours >= 0) {
            std::vector<uint16_t> endPts;
            uint32_t p = start + 10;
            uint16_t total = 0;
            for (int i = 0; i < contours; ++i) {
                total = font->reader.U16(p);
                endPts.push_back(total);
                p += 2;
            }
            const uint16_t instructions = font->reader.U16(p);
            p += 2 + instructions;
            const uint16_t pointCount = total + 1;
            std::vector<uint8_t> flags;
            while (flags.size() < pointCount && p < end) {
                const uint8_t flag = font->reader.U8(p++);
                flags.push_back(flag);
                if (flag & 0x08) {
                    const uint8_t repeat = font->reader.U8(p++);
                    for (uint8_t i = 0; i < repeat; ++i) flags.push_back(flag);
                }
            }
            std::vector<int16_t> xs(pointCount), ys(pointCount);
            int16_t x = 0;
            for (uint16_t i = 0; i < pointCount; ++i) {
                int16_t dx = 0;
                if (flags[i] & 0x02) dx = (flags[i] & 0x10) ? font->reader.U8(p++) : -font->reader.U8(p++);
                else if ((flags[i] & 0x10) == 0) { dx = font->reader.I16(p); p += 2; }
                x = static_cast<int16_t>(x + dx);
                xs[i] = x;
            }
            int16_t y = 0;
            for (uint16_t i = 0; i < pointCount; ++i) {
                int16_t dy = 0;
                if (flags[i] & 0x04) dy = (flags[i] & 0x20) ? font->reader.U8(p++) : -font->reader.U8(p++);
                else if ((flags[i] & 0x20) == 0) { dy = font->reader.I16(p); p += 2; }
                y = static_cast<int16_t>(y + dy);
                ys[i] = y;
            }
            uint16_t begin = 0;
            for (uint16_t endPt : endPts) {
                std::vector<Vec2> raw;
                std::vector<bool> on;
                for (uint16_t i = begin; i <= endPt; ++i) {
                    const float px = xs[i];
                    const float py = ys[i];
                    raw.push_back(Vec2{px * a + py * c + tx, px * b + py * d + ty});
                    on.push_back((flags[i] & 0x01) != 0);
                }
                std::vector<Vec2> flattened;
                FlattenContour(raw, on, flattened);
                if (!flattened.empty()) outline.contours.push_back(std::move(flattened));
                begin = endPt + 1;
            }
        } else {
            uint32_t p = start + 10;
            bool more = true;
            while (more && p + 4 <= end) {
                const uint16_t flags = font->reader.U16(p); p += 2;
                const uint16_t component = font->reader.U16(p); p += 2;
                int arg1 = 0, arg2 = 0;
                if (flags & 0x0001) { arg1 = font->reader.I16(p); arg2 = font->reader.I16(p + 2); p += 4; }
                else { arg1 = static_cast<int8_t>(font->reader.U8(p)); arg2 = static_cast<int8_t>(font->reader.U8(p + 1)); p += 2; }
                float dx = (flags & 0x0002) ? static_cast<float>(arg1) : 0.0f;
                float dy = (flags & 0x0002) ? static_cast<float>(arg2) : 0.0f;
                float ma = 1, mb = 0, mc = 0, md = 1;
                if (flags & 0x0008) { ma = md = font->reader.I16(p) / 16384.0f; p += 2; }
                else if (flags & 0x0040) { ma = font->reader.I16(p) / 16384.0f; md = font->reader.I16(p + 2) / 16384.0f; p += 4; }
                else if (flags & 0x0080) {
                    ma = font->reader.I16(p) / 16384.0f; mb = font->reader.I16(p + 2) / 16384.0f;
                    mc = font->reader.I16(p + 4) / 16384.0f; md = font->reader.I16(p + 6) / 16384.0f; p += 8;
                }
                self(self, component, tx + dx * a + dy * c, ty + dx * b + dy * d,
                     a * ma + c * mb, b * ma + d * mb, a * mc + c * md, b * mc + d * md);
                more = (flags & 0x0020) != 0;
            }
        }
        seen.erase(gid);
        return true;
    };
    outline.xMin = outline.yMin = std::numeric_limits<int16_t>::max();
    outline.xMax = outline.yMax = std::numeric_limits<int16_t>::min();
    loadOutline(loadOutline, glyphId, 0, 0, 1, 0, 0, 1);
    outline.advanceWidth = glyphId < font->advanceWidths.size() ? font->advanceWidths[glyphId] : font->unitsPerEm / 2;
    if (!RasterizeGlyph(*font, outline, fontSizePx, outGlyph)) return false;
    font->glyphCache[cacheKey] = outGlyph;
    return true;
}

bool CoreTextEngine::RasterizeGlyph(const ParsedFont& font, const GlyphOutline& outline, float fontSizePx, GlyphBitmap& outGlyph) const {
    const float scale = fontSizePx / font.unitsPerEm;
    if (outline.contours.empty()) {
        outGlyph.width = outGlyph.height = 0;
        outGlyph.advanceX = static_cast<int32_t>(outline.advanceWidth * scale);
        return true;
    }
    outGlyph.bearingX = static_cast<int32_t>(std::floor(outline.xMin * scale));
    outGlyph.bearingY = static_cast<int32_t>(std::ceil(outline.yMax * scale));
    outGlyph.width = static_cast<uint32_t>(std::max(1.0f, std::ceil((outline.xMax - outline.xMin) * scale) + 2));
    outGlyph.height = static_cast<uint32_t>(std::max(1.0f, std::ceil((outline.yMax - outline.yMin) * scale) + 2));
    outGlyph.advanceX = static_cast<int32_t>(std::max(1.0f, outline.advanceWidth * scale));
    outGlyph.alpha.assign(static_cast<size_t>(outGlyph.width) * outGlyph.height, 0);

    std::vector<std::vector<Vec2>> pixelContours;
    for (const auto& contour : outline.contours) {
        std::vector<Vec2> c;
        for (const auto& p : contour) {
            c.push_back(Vec2{p.x * scale - outGlyph.bearingX, outGlyph.bearingY - p.y * scale});
        }
        pixelContours.push_back(std::move(c));
    }
    constexpr int samples = 4;
    for (uint32_t y = 0; y < outGlyph.height; ++y) {
        for (uint32_t x = 0; x < outGlyph.width; ++x) {
            int covered = 0;
            for (int sy = 0; sy < samples; ++sy) {
                for (int sx = 0; sx < samples; ++sx) {
                    if (PointInside(pixelContours, x + (sx + 0.5f) / samples, y + (sy + 0.5f) / samples)) ++covered;
                }
            }
            outGlyph.alpha[static_cast<size_t>(y) * outGlyph.width + x] = static_cast<uint8_t>(covered * 255 / (samples * samples));
        }
    }
    return true;
}

std::vector<uint32_t> CoreTextEngine::DecodeUtf16(const String& text) const {
    std::vector<uint32_t> codepoints;
    for (size_t i = 0; i < text.size(); ++i) {
        const uint32_t ch = static_cast<uint32_t>(text[i]);
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < text.size()) {
            const uint32_t low = static_cast<uint32_t>(text[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codepoints.push_back(0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00));
                ++i;
                continue;
            }
        }
        codepoints.push_back(ch);
    }
    return codepoints;
}

String CoreTextEngine::NormalizeFamily(String value) const {
    value.erase(std::remove_if(value.begin(), value.end(), [](wchar_t ch) { return ch == L' ' || ch == L'\t'; }), value.end());
    for (auto& ch : value) ch = static_cast<wchar_t>(std::towlower(ch));
    return value;
}

} // namespace CloverPic::CoreText
