#include "Platform/Mac/MacPngCodec.h"

#include <zlib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace CloverPic::Platform::Mac::Png {

namespace {

constexpr uint8_t PngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

void PushBe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t ReadBe32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

void PushChunk(std::vector<uint8_t>& out, const char type[5], const std::vector<uint8_t>& payload) {
    PushBe32(out, static_cast<uint32_t>(payload.size()));
    const size_t crcStart = out.size();
    out.push_back(static_cast<uint8_t>(type[0]));
    out.push_back(static_cast<uint8_t>(type[1]));
    out.push_back(static_cast<uint8_t>(type[2]));
    out.push_back(static_cast<uint8_t>(type[3]));
    out.insert(out.end(), payload.begin(), payload.end());
    uLong crc = crc32(0u, out.data() + crcStart, static_cast<uInt>(4 + payload.size()));
    PushBe32(out, static_cast<uint32_t>(crc));
}

bool Deflate(const std::vector<uint8_t>& raw, std::vector<uint8_t>& outCompressed) {
    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    outCompressed.resize(bound);
    const int status = compress2(outCompressed.data(), &bound, raw.data(),
                                 static_cast<uLong>(raw.size()), 6);
    if (status != Z_OK) {
        outCompressed.clear();
        return false;
    }
    outCompressed.resize(bound);
    return true;
}

bool Inflate(const std::vector<uint8_t>& compressed, size_t expectedSize, std::vector<uint8_t>& outRaw) {
    outRaw.assign(expectedSize, 0);
    z_stream stream = {};
    if (inflateInit(&stream) != Z_OK) return false;

    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = outRaw.data();
    stream.avail_out = static_cast<uInt>(outRaw.size());
    const int status = inflate(&stream, Z_FINISH);
    const size_t produced = outRaw.size() - stream.avail_out;
    inflateEnd(&stream);

    if (status != Z_STREAM_END && status != Z_OK) return false;
    outRaw.resize(produced);
    return produced == expectedSize;
}

// Serializes filtered scanlines (filter type 0 / None) and wraps them in a PNG
// container with a 16 bit RGBA or 8 bit RGBA image header.
bool EncodeRgba(uint32_t width, uint32_t height, const std::vector<uint8_t>& rowBytes,
                uint8_t bitDepth, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    if (width == 0 || height == 0) return false;

    const size_t stride = rowBytes.size() / height;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * height);
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0); // filter: None
        raw.insert(raw.end(), rowBytes.begin() + static_cast<size_t>(y) * stride,
                   rowBytes.begin() + static_cast<size_t>(y + 1) * stride);
    }

    std::vector<uint8_t> compressed;
    if (!Deflate(raw, compressed)) return false;

    outBytes.insert(outBytes.end(), std::begin(PngSignature), std::end(PngSignature));

    std::vector<uint8_t> ihdr;
    ihdr.reserve(13);
    PushBe32(ihdr, width);
    PushBe32(ihdr, height);
    ihdr.push_back(bitDepth);
    ihdr.push_back(6); // colour type: truecolour with alpha
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter method
    ihdr.push_back(0); // interlace: none
    PushChunk(outBytes, "IHDR", ihdr);
    PushChunk(outBytes, "IDAT", compressed);
    PushChunk(outBytes, "IEND", {});
    return true;
}

uint8_t Paeth(uint8_t a, uint8_t b, uint8_t c) {
    const int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    const int pa = std::abs(p - static_cast<int>(a));
    const int pb = std::abs(p - static_cast<int>(b));
    const int pc = std::abs(p - static_cast<int>(c));
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

bool UnfilterRow(uint8_t filter, uint8_t* row, const uint8_t* previousRow, size_t stride, size_t bpp) {
    switch (filter) {
        case 0:
            return true;
        case 1:
            for (size_t i = bpp; i < stride; ++i) row[i] = static_cast<uint8_t>(row[i] + row[i - bpp]);
            return true;
        case 2:
            if (!previousRow) return true;
            for (size_t i = 0; i < stride; ++i) row[i] = static_cast<uint8_t>(row[i] + previousRow[i]);
            return true;
        case 3:
            for (size_t i = 0; i < stride; ++i) {
                const uint16_t left = i >= bpp ? row[i - bpp] : 0;
                const uint16_t up = previousRow ? previousRow[i] : 0;
                row[i] = static_cast<uint8_t>(row[i] + ((left + up) >> 1));
            }
            return true;
        case 4:
            for (size_t i = 0; i < stride; ++i) {
                const uint8_t left = i >= bpp ? row[i - bpp] : 0;
                const uint8_t up = previousRow ? previousRow[i] : 0;
                const uint8_t upLeft = (previousRow && i >= bpp) ? previousRow[i - bpp] : 0;
                row[i] = static_cast<uint8_t>(row[i] + Paeth(left, up, upLeft));
            }
            return true;
        default:
            return false;
    }
}

} // namespace

bool EncodeBgra8(uint32_t width, uint32_t height, const std::vector<uint8_t>& bgraPixels,
                 std::vector<uint8_t>& outBytes) {
    if (width == 0 || height == 0) return false;
    if (bgraPixels.size() < static_cast<size_t>(width) * height * 4) return false;

    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i + 3 < bgraPixels.size(); i += 4) {
        rgba[i + 0] = bgraPixels[i + 2];
        rgba[i + 1] = bgraPixels[i + 1];
        rgba[i + 2] = bgraPixels[i + 0];
        rgba[i + 3] = bgraPixels[i + 3];
    }
    return EncodeRgba(width, height, rgba, 8, outBytes);
}

bool EncodeRgba16(uint32_t width, uint32_t height, const std::vector<uint16_t>& rgba16Pixels,
                  std::vector<uint8_t>& outBytes) {
    if (width == 0 || height == 0) return false;
    if (rgba16Pixels.size() < static_cast<size_t>(width) * height * 4) return false;

    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint8_t> bytes(pixelCount * 8);
    for (size_t i = 0; i < pixelCount * 4; ++i) {
        // PNG stores 16 bit samples big-endian.
        bytes[i * 2 + 0] = static_cast<uint8_t>((rgba16Pixels[i] >> 8) & 0xFF);
        bytes[i * 2 + 1] = static_cast<uint8_t>(rgba16Pixels[i] & 0xFF);
    }
    return EncodeRgba(width, height, bytes, 16, outBytes);
}

bool DecodeRgba16(const std::vector<uint8_t>& bytes, uint32_t& outWidth, uint32_t& outHeight,
                  std::vector<uint16_t>& outRgba16Pixels) {
    outWidth = 0;
    outHeight = 0;
    outRgba16Pixels.clear();

    if (bytes.size() < 8 || std::memcmp(bytes.data(), PngSignature, 8) != 0) return false;

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bitDepth = 0;
    uint8_t colorType = 0;
    uint8_t interlace = 0;
    bool haveHeader = false;
    std::vector<uint8_t> idat;

    size_t offset = 8;
    while (offset + 8 <= bytes.size()) {
        const uint32_t length = ReadBe32(bytes.data() + offset);
        const char* type = reinterpret_cast<const char*>(bytes.data() + offset + 4);
        const size_t dataStart = offset + 8;
        if (dataStart + length + 4 > bytes.size()) return false;
        const uint8_t* data = bytes.data() + dataStart;

        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (length < 13) return false;
            width = ReadBe32(data + 0);
            height = ReadBe32(data + 4);
            bitDepth = data[8];
            colorType = data[9];
            interlace = data[12];
            haveHeader = true;
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + length);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }

        offset = dataStart + length + 4;
    }

    if (!haveHeader || width == 0 || height == 0 || idat.empty()) return false;
    if (interlace != 0) return false;
    if (bitDepth != 8 && bitDepth != 16) return false;

    size_t channels = 0;
    switch (colorType) {
        case 0: channels = 1; break; // grayscale
        case 2: channels = 3; break; // RGB
        case 4: channels = 2; break; // grayscale + alpha
        case 6: channels = 4; break; // RGBA
        default: return false;       // palette and other types are not project data
    }

    const size_t bytesPerSample = bitDepth / 8;
    const size_t stride = static_cast<size_t>(width) * channels * bytesPerSample;
    const size_t expected = (stride + 1) * static_cast<size_t>(height);

    std::vector<uint8_t> raw;
    if (!Inflate(idat, expected, raw)) return false;

    const size_t bpp = std::max<size_t>(1, channels * bytesPerSample);
    const bool sixteenBit = bitDepth == 16;

    // `raw` outlives the loop, so the previous (already unfiltered) scanline can
    // be referenced in place.
    const uint8_t* previous = nullptr;
    outRgba16Pixels.assign(static_cast<size_t>(width) * height * 4, 0);

    for (uint32_t y = 0; y < height; ++y) {
        const size_t rowStart = static_cast<size_t>(y) * (stride + 1);
        const uint8_t filter = raw[rowStart];
        uint8_t* row = raw.data() + rowStart + 1;
        if (!UnfilterRow(filter, row, previous, stride, bpp)) return false;

        for (uint32_t x = 0; x < width; ++x) {
            const size_t inIndex = static_cast<size_t>(x) * channels * bytesPerSample;
            const size_t outIndex = (static_cast<size_t>(y) * width + x) * 4;
            const auto sample8 = [&](size_t channel) -> uint16_t {
                const uint8_t v = row[inIndex + channel];
                return static_cast<uint16_t>(v * 257);
            };
            const auto sample16 = [&](size_t channel) -> uint16_t {
                const size_t at = inIndex + channel * 2;
                return static_cast<uint16_t>((row[at] << 8) | row[at + 1]);
            };

            switch (colorType) {
                case 6: {
                    for (size_t c = 0; c < 4; ++c) {
                        outRgba16Pixels[outIndex + c] = sixteenBit ? sample16(c) : sample8(c);
                    }
                    break;
                }
                case 2: {
                    for (size_t c = 0; c < 3; ++c) {
                        outRgba16Pixels[outIndex + c] = sixteenBit ? sample16(c) : sample8(c);
                    }
                    outRgba16Pixels[outIndex + 3] = 0xFFFF;
                    break;
                }
                case 4: {
                    const uint16_t gray = sixteenBit ? sample16(0) : sample8(0);
                    const uint16_t alpha = sixteenBit ? sample16(1) : sample8(1);
                    outRgba16Pixels[outIndex + 0] = gray;
                    outRgba16Pixels[outIndex + 1] = gray;
                    outRgba16Pixels[outIndex + 2] = gray;
                    outRgba16Pixels[outIndex + 3] = alpha;
                    break;
                }
                case 0:
                default: {
                    const uint16_t gray = sixteenBit ? sample16(0) : sample8(0);
                    outRgba16Pixels[outIndex + 0] = gray;
                    outRgba16Pixels[outIndex + 1] = gray;
                    outRgba16Pixels[outIndex + 2] = gray;
                    outRgba16Pixels[outIndex + 3] = 0xFFFF;
                    break;
                }
            }
        }

        previous = row;
    }

    outWidth = width;
    outHeight = height;
    return true;
}

} // namespace CloverPic::Platform::Mac::Png
