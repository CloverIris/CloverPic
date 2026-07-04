#pragma once

#include "Core/Services/CoreServices.h"

namespace CloverPic {

class WindowsFileSystem final : public IPlatformFileSystem {
public:
    bool ReadFileBytes(const String& filePath, std::vector<uint8_t>& outBytes) override;
    bool WriteFileBytes(const String& filePath, const std::vector<uint8_t>& bytes) override;
};

class WindowsImageEncoder final : public IImageEncoder {
public:
    bool EncodePngBgra(const String& filePath, uint32_t width, uint32_t height,
                       const std::vector<uint8_t>& bgraPixels) override;
};

} // namespace CloverPic
