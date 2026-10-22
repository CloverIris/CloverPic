#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <CoreGraphics/CoreGraphics.h>

#include "Platform/Mac/MacCoreServices.h"
#include "Platform/Mac/MacPngCodec.h"
#include "Platform/Mac/MacString.h"
#include "Core/Services/CoreServices.h"

#include <mach/mach.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// MacString/MacPngCodec live in CloverPic::Platform::Mac; the services below are
// declared in CloverPic so core sees them platform-neutrally.
using namespace CloverPic::Platform::Mac;

namespace CloverPic {

namespace {

std::string ServiceDataDirectory() {
    NSString* override = [[[NSProcessInfo processInfo] environment] objectForKey:@"CLOVERPIC_DATA_DIR"];
    if (override.length > 0) {
        return std::string([override fileSystemRepresentation]);
    }
    NSArray<NSString*>* bases =
        NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* base = bases.count > 0 ? bases[0] : NSTemporaryDirectory();
    NSString* dir = [base stringByAppendingPathComponent:@"CloverPic"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir
                             withIntermediateDirectories:YES
                                              attributes:nil
                                                   error:nil];
    return std::string([dir fileSystemRepresentation]);
}

NSString* ToNSString(const String& text) {
    const std::string utf8 = ToUtf8(text);
    NSString* result = [NSString stringWithUTF8String:utf8.c_str()];
    return result ? result : @"";
}

String ToCoreString(NSString* text) {
    if (!text) return String();
    const char* utf8 = [text UTF8String];
    return FromUtf8(utf8 ? utf8 : "");
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    if (path.empty()) return false;
    std::ifstream file(path, std::ios::binary | std::ios::in);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);
    outBytes.resize(static_cast<size_t>(size));
    if (!outBytes.empty()) {
        file.read(reinterpret_cast<char*>(outBytes.data()), size);
    }
    return file.good() || file.eof();
}

bool WriteFileBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    if (path.empty()) return false;
    NSString* parent = [ToNSString(FromUtf8(path)) stringByDeletingLastPathComponent];
    if (parent.length > 0) {
        [[NSFileManager defaultManager] createDirectoryAtPath:parent
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil];
    }
    std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return file.good();
}

String Lower(String value) {
    for (auto& ch : value) ch = static_cast<wchar_t>(std::towlower(ch));
    return value;
}

bool EndsWithCloverPic(const String& path) {
    return Lower(path).ends_with(L".cloverpic");
}

// Extracts the display's ICC profile into the app data directory so that core
// can read it through the ordinary file-bytes service.
String EnsureDisplayProfileFile() {
    static String cached;
    if (!cached.empty()) return cached;

    CGColorSpaceRef space = CGDisplayCopyColorSpace(CGMainDisplayID());
    if (!space) {
        space = CGColorSpaceCreateDeviceRGB();
    }
    CFDataRef icc = space ? CGColorSpaceCopyICCData(space) : nullptr;
    if (icc) {
        const auto* bytes = static_cast<const uint8_t*>(CFDataGetBytePtr(icc));
        std::vector<uint8_t> data(bytes, bytes + CFDataGetLength(icc));
        const std::string path = ServiceDataDirectory() + "/display.icc";
        if (WriteFileBytes(path, data)) {
            cached = FromUtf8(path);
        }
        CFRelease(icc);
    }
    if (space) {
        CGColorSpaceRelease(space);
    }
    return cached;
}

class MacMemoryInfoProvider final : public IMemoryInfoProvider {
public:
    uint64_t GetTotalPhysicalMemory() const override {
        uint64_t memory = 0;
        size_t length = sizeof(memory);
        if (sysctlbyname("hw.memsize", &memory, &length, nullptr, 0) != 0) return 0;
        return memory;
    }

    uint64_t GetAvailablePhysicalMemory() const override {
        const mach_port_t host = mach_host_self();
        vm_size_t pageSize = 0;
        if (host_page_size(host, &pageSize) != KERN_SUCCESS) return 0;
        vm_statistics64_data_t stats = {};
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count) != KERN_SUCCESS) {
            return 0;
        }
        const uint64_t freePages = static_cast<uint64_t>(stats.free_count) +
                                   static_cast<uint64_t>(stats.inactive_count) +
                                   static_cast<uint64_t>(stats.speculative_count);
        return freePages * static_cast<uint64_t>(pageSize);
    }
};

class MacDisplayInfoProvider final : public IDisplayInfoProvider {
public:
    DisplayInfo GetPrimaryDisplayInfo() const override {
        DisplayInfo info;
        NSScreen* screen = [NSScreen screens].firstObject;
        if (!screen) return info;

        const CGFloat scale = screen.backingScaleFactor > 0 ? screen.backingScaleFactor : 1.0;
        const NSSize points = screen.frame.size;
        info.primaryDisplayPixels = Size(static_cast<int32_t>(std::llround(points.width * scale)),
                                         static_cast<int32_t>(std::llround(points.height * scale)));
        info.dpiScale = static_cast<float>(scale);
        info.bitsPerPixel = 32;
        info.dynamicRange = screen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0
                                ? DisplayDynamicRange::Hdr
                                : DisplayDynamicRange::Sdr;
        info.colorProfilePath = EnsureDisplayProfileFile();
        return info;
    }
};

class MacRecentFilesStore final : public IRecentFilesStore {
public:
    std::vector<String> LoadRecentFiles() override {
        std::vector<String> files;
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(RecentFilePath(), bytes) || bytes.empty()) return files;

        std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        size_t start = 0;
        while (start < text.size()) {
            size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            std::string line(text.substr(start, end - start));
            start = end + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty()) continue;
            const String path = FromUtf8(line);
            if (EndsWithCloverPic(path)) files.push_back(path);
        }
        return files;
    }

    void SaveRecentFiles(const std::vector<String>& files) override {
        std::string buffer;
        for (const auto& entry : files) {
            if (!EndsWithCloverPic(entry)) continue;
            buffer += ToUtf8(entry);
            buffer.push_back('\n');
        }
        const std::vector<uint8_t> bytes(buffer.begin(), buffer.end());
        WriteFileBytes(RecentFilePath(), bytes);
    }

private:
    static std::string RecentFilePath() {
        return ServiceDataDirectory() + "/recent.txt";
    }
};

class MacFileSystem final : public IPlatformFileSystem {
public:
    bool ReadFileBytes(const String& filePath, std::vector<uint8_t>& outBytes) override {
        return CloverPic::ReadFileBytes(ToFilesystemPath(filePath), outBytes);
    }

    bool WriteFileBytes(const String& filePath, const std::vector<uint8_t>& bytes) override {
        return CloverPic::WriteFileBytes(ToFilesystemPath(filePath), bytes);
    }
};

class MacImageCodec final : public IImageCodec {
public:
    bool EncodeDisplayPngBgra8ToFile(const String& filePath, uint32_t width, uint32_t height,
                                     const std::vector<uint8_t>& bgraPixels) override {
        std::vector<uint8_t> png;
        if (!Png::EncodeBgra8(width, height, bgraPixels, png)) return false;
        return CloverPic::WriteFileBytes(ToFilesystemPath(filePath), png);
    }

    bool EncodePngRgba16ToBytes(uint32_t width, uint32_t height,
                                const std::vector<uint16_t>& rgba16Pixels,
                                std::vector<uint8_t>& outBytes) override {
        return Png::EncodeRgba16(width, height, rgba16Pixels, outBytes);
    }

    bool DecodePngRgba16FromBytes(const std::vector<uint8_t>& bytes, uint32_t& outWidth,
                                  uint32_t& outHeight, std::vector<uint16_t>& outRgba16Pixels) override {
        return Png::DecodeRgba16(bytes, outWidth, outHeight, outRgba16Pixels);
    }
};

class MacFileDialogService final : public IFileDialogService {
public:
    explicit MacFileDialogService(std::function<void*()> windowProvider)
        : m_windowProvider(std::move(windowProvider)) {}

    bool PickOpenProjectPath(String& outPath) override {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.message = @"Open a CloverPic project";
        ApplyProjectType(panel);
        if (!outPath.empty()) {
            NSString* existing = ToNSString(outPath);
            panel.directoryURL = [NSURL fileURLWithPath:[existing stringByDeletingLastPathComponent]];
        }
        if ([panel runModal] != NSModalResponseOK) return false;
        NSString* path = panel.URL.path;
        if (!path) return false;
        outPath = ToCoreString(path);
        return true;
    }

    bool PickSaveProjectPath(String& outPath) override {
        return RunSavePanel(outPath, @"cloverpic", @"Untitled.cloverpic", @"Save CloverPic project");
    }

    bool PickExportImagePath(String& outPath) override {
        return RunSavePanel(outPath, @"png", @"Untitled.png", @"Export image");
    }

    UnsavedChangesChoice PromptUnsavedChanges(const String& actionLabel) override {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleWarning;
        alert.messageText = @"Save changes before continuing?";
        alert.informativeText = [NSString stringWithFormat:
            @"Unsaved edits in the current project will be lost if you continue without saving (%@).",
            ToNSString(actionLabel)];
        [alert addButtonWithTitle:@"Save"];
        [alert addButtonWithTitle:@"Don't Save"];
        [alert addButtonWithTitle:@"Cancel"];

        NSWindow* owner = ActiveWindow();
        if (owner) {
            [NSApp activateIgnoringOtherApps:YES];
        }
        const NSModalResponse response = [alert runModal];

        if (response == NSAlertFirstButtonReturn) return UnsavedChangesChoice::Save;
        if (response == NSAlertSecondButtonReturn) return UnsavedChangesChoice::Discard;
        return UnsavedChangesChoice::Cancel;
    }

private:
    std::function<void*()> m_windowProvider;

    NSWindow* ActiveWindow() const {
        void* raw = m_windowProvider ? m_windowProvider() : nullptr;
        return raw ? (__bridge NSWindow*)raw : nil;
    }

    static void ApplyProjectType(NSSavePanel* panel) {
        UTType* type = [UTType typeWithFilenameExtension:@"cloverpic"];
        if (type) panel.allowedContentTypes = @[type];
    }

    bool RunSavePanel(String& outPath, NSString* extension, NSString* defaultName, NSString* message) {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.message = message;
        panel.canCreateDirectories = YES;
        UTType* type = [UTType typeWithFilenameExtension:extension];
        if (type) panel.allowedContentTypes = @[type];
        if (outPath.empty()) {
            panel.nameFieldStringValue = defaultName;
            NSString* documents = NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
            panel.directoryURL = [NSURL fileURLWithPath:(documents ? documents : NSHomeDirectory())
                                            isDirectory:YES];
        } else {
            NSString* existing = ToNSString(outPath);
            panel.directoryURL = [NSURL fileURLWithPath:[existing stringByDeletingLastPathComponent]
                                            isDirectory:YES];
            panel.nameFieldStringValue = [existing lastPathComponent];
        }
        if ([panel runModal] != NSModalResponseOK) return false;
        NSString* path = panel.URL.path;
        if (!path) return false;
        outPath = ToCoreString(path);
        return true;
    }
};

class MacFontCatalogProvider final : public IPlatformFontCatalogProvider {
public:
    PlatformFontCatalog LoadFontCatalog() override {
        PlatformFontCatalog catalog;
        std::set<std::string> uniquePaths;
        for (NSString* directory : FontDirectories()) {
            CollectFontFiles(directory, uniquePaths);
        }

        for (const auto& path : uniquePaths) {
            catalog.fonts.push_back({FromUtf8(path)});
        }

        ChooseSystemUiFamily(catalog, uniquePaths);
        return catalog;
    }

private:
    static NSArray<NSString*>* FontDirectories() {
        NSMutableArray<NSString*>* directories = [NSMutableArray arrayWithObjects:
            @"/System/Library/Fonts",
            @"/Library/Fonts",
            [@"~/Library/Fonts" stringByExpandingTildeInPath],
            nil];
        return directories;
    }

    static void CollectFontFiles(NSString* directory, std::set<std::string>& outPaths) {
        NSArray<NSString*>* entries =
            [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:nil];
        if (!entries) return;
        for (NSString* name in entries) {
            NSString* lower = [name lowercaseString];
            if (![lower hasSuffix:@".ttf"] && ![lower hasSuffix:@".ttc"]) continue;
            NSString* full = [directory stringByAppendingPathComponent:name];
            const char* utf8 = [full UTF8String];
            if (utf8) outPaths.insert(utf8);
        }
    }

    // Core rasterizes text with its own TrueType (`glyf`) parser, so the system
    // UI family must point at a face that has TrueType outlines. SF Pro ships as
    // CFF/OTF on macOS, which core cannot outline, hence the ordered candidates.
    static void ChooseSystemUiFamily(PlatformFontCatalog& catalog, const std::set<std::string>& paths) {
        struct Candidate {
            const char* fileHint;
            const char* family;
        };
        static constexpr Candidate candidates[] = {
            {"helveticaneue.ttc", "Helvetica Neue"},
            {"helvetica.ttc", "Helvetica"},
            {"geneva.ttf", "Geneva"},
            {"arial.ttf", "Arial"},
        };
        for (const auto& candidate : candidates) {
            for (const auto& path : paths) {
                const std::string lower = ToLower(path);
                if (lower.find(candidate.fileHint) == std::string::npos) continue;
                catalog.systemUiFamily = FromUtf8(candidate.family);
                catalog.systemUiFontFileHint = FromUtf8(path);
                return;
            }
        }
        if (!catalog.fonts.empty()) {
            catalog.systemUiFontFileHint = catalog.fonts.front().path;
        }
    }

    static std::string ToLower(std::string value) {
        for (auto& ch : value) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }
};

class MacColorProfileProvider final : public IColorProfileProvider {
public:
    std::vector<ColorProfileInfo> EnumerateColorProfiles() override {
        std::vector<ColorProfileInfo> profiles;
        std::set<String> seen;

        const ColorProfileInfo current = GetCurrentDisplayProfile();
        if (!current.path.empty()) {
            profiles.push_back(current);
            seen.insert(Lower(current.path));
        }

        for (NSString* directory : ProfileDirectories()) {
            NSArray<NSString*>* entries =
                [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:nil];
            if (!entries) continue;
            std::vector<std::string> sorted;
            for (NSString* name in entries) {
                NSString* lower = [name lowercaseString];
                if (![lower hasSuffix:@".icc"] && ![lower hasSuffix:@".icm"]) continue;
                NSString* full = [directory stringByAppendingPathComponent:name];
                const char* utf8 = [full UTF8String];
                if (utf8) sorted.push_back(utf8);
            }
            std::sort(sorted.begin(), sorted.end());
            for (const auto& path : sorted) {
                const String corePath = FromUtf8(path);
                if (!seen.insert(Lower(corePath)).second) continue;
                ColorProfileInfo info;
                info.path = corePath;
                info.id = corePath;
                info.displayName = FromUtf8(std::filesystem::path(path).filename().string());
                info.isCurrentDisplayProfile = false;
                info.isDefault = false;
                profiles.push_back(info);
            }
        }

        if (profiles.empty()) {
            ColorProfileInfo fallback;
            fallback.id = L"srgb";
            fallback.displayName = L"sRGB fallback";
            fallback.isDefault = true;
            profiles.push_back(fallback);
        }
        return profiles;
    }

    ColorProfileInfo GetCurrentDisplayProfile() override {
        ColorProfileInfo info;
        info.id = L"current-display";
        info.displayName = L"Current display profile";
        info.path = EnsureDisplayProfileFile();
        if (!info.path.empty()) {
            info.id = info.path;
            const size_t slash = info.path.find_last_of(L"/\\");
            info.displayName = slash == String::npos ? info.path : info.path.substr(slash + 1);
        }
        info.isCurrentDisplayProfile = true;
        info.isDefault = true;
        return info;
    }

    bool ReadProfileBytes(const String& path, std::vector<uint8_t>& outBytes) override {
        outBytes.clear();
        if (path.empty()) return false;
        return CloverPic::ReadFileBytes(ToFilesystemPath(path), outBytes);
    }

private:
    static NSArray<NSString*>* ProfileDirectories() {
        return @[
            @"/System/Library/ColorSync/Profiles",
            @"/Library/ColorSync/Profiles",
            [@"~/Library/ColorSync/Profiles" stringByExpandingTildeInPath],
        ];
    }
};

class MacAppSettingsStore final : public IAppSettingsStore {
public:
    bool LoadSettingsBytes(std::vector<uint8_t>& outBytes) override {
        return CloverPic::ReadFileBytes(SettingsPath(), outBytes);
    }

    bool SaveSettingsBytes(const std::vector<uint8_t>& bytes) override {
        return CloverPic::WriteFileBytes(SettingsPath(), bytes);
    }

private:
    static std::string SettingsPath() {
        return ServiceDataDirectory() + "/settings.bin";
    }
};

} // namespace

} // namespace CloverPic

namespace CloverPic::Platform::Mac {

void RegisterMacCoreServices(std::function<void*()> activeWindowProvider) {
    PlatformServices services;
    services.memoryInfoProvider = MakeRef<MacMemoryInfoProvider>();
    services.displayInfoProvider = MakeRef<MacDisplayInfoProvider>();
    services.recentFilesStore = MakeRef<MacRecentFilesStore>();
    services.fileSystem = MakeRef<MacFileSystem>();
    services.imageCodec = MakeRef<MacImageCodec>();
    services.fileDialogService = MakeRef<MacFileDialogService>(std::move(activeWindowProvider));
    services.fontCatalogProvider = MakeRef<MacFontCatalogProvider>();
    services.colorProfileProvider = MakeRef<MacColorProfileProvider>();
    services.appSettingsStore = MakeRef<MacAppSettingsStore>();
    CoreServices::InstallPlatformServices(std::move(services));
}

const char* MacDataDirectoryPath() {
    static const std::string directory = ServiceDataDirectory();
    return directory.c_str();
}

} // namespace CloverPic::Platform::Mac
