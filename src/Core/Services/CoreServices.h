#pragma once

#include "Utils/Types.h"
#include <cstdint>
#include <vector>

namespace CloverPic {

enum class DisplayDynamicRange {
    Unknown,
    Sdr,
    Hdr
};

struct DisplayInfo {
    Size primaryDisplayPixels;
    float dpiScale = 1.0f;
    uint32_t bitsPerPixel = 0;
    DisplayDynamicRange dynamicRange = DisplayDynamicRange::Unknown;
    String colorProfilePath;
};

struct ColorProfileInfo {
    String id;
    String displayName;
    String path;
    bool isDefault = false;
    bool isCurrentDisplayProfile = false;
};

struct TextRasterRequest {
    String text;
    String fontFamily;
    float fontSize = 24.0f;
    Color color = Color(255, 255, 255, 255);
    uint32_t maxWidth = 0;
    uint32_t maxHeight = 0;
};

struct RasterizedTextBitmap {
    uint32_t width = 0;
    uint32_t height = 0;
    int32_t baseline = 0;
    int32_t advanceX = 0;
    std::vector<uint8_t> rgbaPixels;

    bool IsEmpty() const {
        return width == 0 || height == 0 || rgbaPixels.empty();
    }
};

struct PlatformFontInfo {
    String path;
};

struct PlatformFontCatalog {
    String systemUiFamily;
    String systemUiFontFileHint;
    std::vector<PlatformFontInfo> fonts;
};

class IMemoryInfoProvider {
public:
    virtual ~IMemoryInfoProvider() = default;
    virtual uint64_t GetTotalPhysicalMemory() const = 0;
    virtual uint64_t GetAvailablePhysicalMemory() const = 0;
};

class IDisplayInfoProvider {
public:
    virtual ~IDisplayInfoProvider() = default;
    virtual DisplayInfo GetPrimaryDisplayInfo() const = 0;
};

class IRecentFilesStore {
public:
    virtual ~IRecentFilesStore() = default;
    virtual std::vector<String> LoadRecentFiles() = 0;
    virtual void SaveRecentFiles(const std::vector<String>& files) = 0;
};

class IPlatformFileSystem {
public:
    virtual ~IPlatformFileSystem() = default;
    virtual bool ReadFileBytes(const String& filePath, std::vector<uint8_t>& outBytes) = 0;
    virtual bool WriteFileBytes(const String& filePath, const std::vector<uint8_t>& bytes) = 0;
};

class IImageCodec {
public:
    virtual ~IImageCodec() = default;
    virtual bool EncodeDisplayPngBgra8ToFile(const String& filePath, uint32_t width, uint32_t height,
                                             const std::vector<uint8_t>& bgraPixels) = 0;
    virtual bool EncodePngRgba16ToBytes(uint32_t width, uint32_t height,
                                        const std::vector<uint16_t>& rgba16Pixels,
                                        std::vector<uint8_t>& outBytes) = 0;
    virtual bool DecodePngRgba16FromBytes(const std::vector<uint8_t>& bytes,
                                          uint32_t& outWidth,
                                          uint32_t& outHeight,
                                          std::vector<uint16_t>& outRgba16Pixels) = 0;
};

enum class UnsavedChangesChoice {
    Save,
    Discard,
    Cancel
};

class IFileDialogService {
public:
    virtual ~IFileDialogService() = default;
    virtual bool PickOpenProjectPath(String& outPath) = 0;
    virtual bool PickSaveProjectPath(String& outPath) = 0;
    virtual bool PickExportImagePath(String& outPath) = 0;
    virtual UnsavedChangesChoice PromptUnsavedChanges(const String& actionLabel) = 0;
};

class IPlatformFontCatalogProvider {
public:
    virtual ~IPlatformFontCatalogProvider() = default;
    virtual PlatformFontCatalog LoadFontCatalog() = 0;
};

class IColorProfileProvider {
public:
    virtual ~IColorProfileProvider() = default;
    virtual std::vector<ColorProfileInfo> EnumerateColorProfiles() = 0;
    virtual ColorProfileInfo GetCurrentDisplayProfile() = 0;
    virtual bool ReadProfileBytes(const String& path, std::vector<uint8_t>& outBytes) = 0;
};

class IAppSettingsStore {
public:
    virtual ~IAppSettingsStore() = default;
    virtual bool LoadSettingsBytes(std::vector<uint8_t>& outBytes) = 0;
    virtual bool SaveSettingsBytes(const std::vector<uint8_t>& bytes) = 0;
};

struct PlatformServices {
    Ref<IMemoryInfoProvider> memoryInfoProvider;
    Ref<IDisplayInfoProvider> displayInfoProvider;
    Ref<IRecentFilesStore> recentFilesStore;
    Ref<IPlatformFileSystem> fileSystem;
    Ref<IImageCodec> imageCodec;
    Ref<IFileDialogService> fileDialogService;
    Ref<IPlatformFontCatalogProvider> fontCatalogProvider;
    Ref<IColorProfileProvider> colorProfileProvider;
    Ref<IAppSettingsStore> appSettingsStore;
};

class CoreServices {
public:
    static void InstallPlatformServices(PlatformServices services);
    static IMemoryInfoProvider* GetMemoryInfoProvider();
    static IDisplayInfoProvider* GetDisplayInfoProvider();
    static IRecentFilesStore* GetRecentFilesStore();
    static IPlatformFileSystem* GetFileSystem();
    static IImageCodec* GetImageCodec();
    static IFileDialogService* GetFileDialogService();
    static IPlatformFontCatalogProvider* GetFontCatalogProvider();
    static IColorProfileProvider* GetColorProfileProvider();
    static IAppSettingsStore* GetAppSettingsStore();
};

} // namespace CloverPic
