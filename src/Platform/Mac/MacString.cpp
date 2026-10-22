#include "Platform/Mac/MacString.h"

#include <cstdint>

namespace CloverPic::Platform::Mac {

namespace {

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        cp = 0xFFFD;
    }
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void AppendUtf16(String& out, uint32_t cp) {
    if (cp > 0x10FFFF) {
        cp = 0xFFFD;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        // Core stores raw UTF-16 code units; a lone surrogate is kept as-is.
        out.push_back(static_cast<wchar_t>(cp));
        return;
    }
    if (cp < 0x10000) {
        out.push_back(static_cast<wchar_t>(cp));
        return;
    }
    cp -= 0x10000;
    out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
    out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
}

} // namespace

std::string ToUtf8(const String& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t cp = static_cast<uint32_t>(text[i]) & 0x1FFFFFu;
        // Recombine surrogate pairs that core may have stored as two units.
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.size()) {
            const uint32_t low = static_cast<uint32_t>(text[i + 1]) & 0x1FFFFFu;
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        AppendUtf8(out, cp);
    }
    return out;
}

String FromUtf8(const std::string& text) {
    String out;
    out.reserve(text.size());
    size_t i = 0;
    const size_t size = text.size();
    while (i < size) {
        const uint8_t lead = static_cast<uint8_t>(text[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (lead < 0x80) {
            cp = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            cp = lead & 0x1F;
            extra = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0F;
            extra = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            cp = lead & 0x07;
            extra = 3;
        } else {
            ++i;
            AppendUtf16(out, 0xFFFD);
            continue;
        }
        if (i + extra >= size) {
            AppendUtf16(out, 0xFFFD);
            break;
        }
        bool valid = true;
        for (size_t k = 1; k <= extra; ++k) {
            const uint8_t next = static_cast<uint8_t>(text[i + k]);
            if ((next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (next & 0x3F);
        }
        if (!valid) {
            ++i;
            AppendUtf16(out, 0xFFFD);
            continue;
        }
        AppendUtf16(out, cp);
        i += extra + 1;
    }
    return out;
}

std::string ToFilesystemPath(const String& path) {
    return ToUtf8(path);
}

} // namespace CloverPic::Platform::Mac
