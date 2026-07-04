#pragma once

#include "Core/Services/CoreServices.h"

namespace CloverPic {

class WindowsFileSystem final : public IPlatformFileSystem {
public:
    bool ReadFileBytes(const String& filePath, std::vector<uint8_t>& outBytes) override;
    bool WriteFileBytes(const String& filePath, const std::vector<uint8_t>& bytes) override;
};

class WindowsImageCodec final : public IImageCodec {
public:
    bool EncodeDisplayPngBgra8ToFile(const String& filePath, uint32_t width, uint32_t height,
                                     const std::vector<uint8_t>& bgraPixels) override;
    bool EncodePngRgba16ToBytes(uint32_t width, uint32_t height,
                                const std::vector<uint16_t>& rgba16Pixels,
                                std::vector<uint8_t>& outBytes) override;
    bool DecodePngRgba16FromBytes(const std::vector<uint8_t>& bytes,
                                  uint32_t& outWidth,
                                  uint32_t& outHeight,
                                  std::vector<uint16_t>& outRgba16Pixels) override;
};

} // namespace CloverPic
