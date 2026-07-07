#include "Platform/Windows/WindowsCoreServices.h"
#include "Core/Services/CoreServices.h"
#include "Platform/Windows/WindowsStorageServices.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <fstream>
#include <set>
#include <shlobj.h>
#include <vector>
#include <windows.h>
#include <commdlg.h>

namespace CloverPic {

namespace {

class WindowsMemoryInfoProvider final : public IMemoryInfoProvider {
public:
    uint64_t GetTotalPhysicalMemory() const override {
        MEMORYSTATUSEX memStatus = {};
        memStatus.dwLength = sizeof(memStatus);
        return GlobalMemoryStatusEx(&memStatus) ? memStatus.ullTotalPhys : 0;
    }

    uint64_t GetAvailablePhysicalMemory() const override {
        MEMORYSTATUSEX memStatus = {};
        memStatus.dwLength = sizeof(memStatus);
        return GlobalMemoryStatusEx(&memStatus) ? memStatus.ullAvailPhys : 0;
    }
};

class WindowsDisplayInfoProvider final : public IDisplayInfoProvider {
public:
    DisplayInfo GetPrimaryDisplayInfo() const override {
        DisplayInfo info;
        HDC hdc = GetDC(nullptr);
        if (!hdc) {
            return info;
        }

        info.primaryDisplayPixels = Size(GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES));
        const int logicalDpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        info.dpiScale = logicalDpiX > 0 ? static_cast<float>(logicalDpiX) / 96.0f : 1.0f;
        info.bitsPerPixel = static_cast<uint32_t>(GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES));

        DWORD profileSize = 0;
        if (!GetICMProfileW(hdc, &profileSize, nullptr) && profileSize > 0) {
            std::vector<wchar_t> profile(profileSize + 1, L'\0');
            if (GetICMProfileW(hdc, &profileSize, profile.data())) {
                info.colorProfilePath = profile.data();
            }
        }

        info.dynamicRange = DisplayDynamicRange::Unknown;
        ReleaseDC(nullptr, hdc);
        return info;
    }
};

class WindowsRecentFilesStore final : public IRecentFilesStore {
public:
    std::vector<String> LoadRecentFiles() override {
        std::vector<String> files;
        String path = GetStoragePath();
        if (path.empty()) return files;

        std::wifstream file(path.c_str());
        if (!file.is_open()) return files;

        String line;
        while (std::getline(file, line)) {
            String lower = line;
            for (auto& ch : lower) ch = static_cast<wchar_t>(std::towlower(ch));
            if (!line.empty() && lower.ends_with(L".cloverpic")) {
                files.push_back(line);
            }
        }
        return files;
    }

    void SaveRecentFiles(const std::vector<String>& files) override {
        String path = GetStoragePath();
        if (path.empty()) return;

        std::wofstream file(path.c_str());
        if (!file.is_open()) return;

        for (const auto& entry : files) {
            String lower = entry;
            for (auto& ch : lower) ch = static_cast<wchar_t>(std::towlower(ch));
            if (!lower.ends_with(L".cloverpic")) continue;
            file << entry << L"\n";
        }
    }

private:
    String GetStoragePath() const {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
            String result = path;
            result += L"\\CloverPic";
            CreateDirectoryW(result.c_str(), nullptr);
            result += L"\\recent.txt";
            return result;
        }
        return L"";
    }
};

class WindowsFileDialogService final : public IFileDialogService {
public:
    explicit WindowsFileDialogService(std::function<HWND()> ownerProvider) : m_ownerProvider(std::move(ownerProvider)) {}

    bool PickOpenProjectPath(String& outPath) override {
        return ShowDialog(
            outPath,
            L"CloverPic Project (*.cloverpic)\0*.cloverpic\0",
            L"cloverpic",
            OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
            false);
    }

    bool PickSaveProjectPath(String& outPath) override {
        return ShowDialog(
            outPath,
            L"CloverPic Project (*.cloverpic)\0*.cloverpic\0",
            L"cloverpic",
            OFN_OVERWRITEPROMPT,
            true);
    }

    bool PickExportImagePath(String& outPath) override {
        return ShowDialog(
            outPath,
            L"PNG Image (*.png)\0*.png\0",
            L"png",
            OFN_OVERWRITEPROMPT,
            true);
    }

    UnsavedChangesChoice PromptUnsavedChanges(const String& actionLabel) override {
        String message = L"Save changes before " + actionLabel + L"?\n\nUnsaved edits in the current project will be lost if you continue without saving.";
        const int result = MessageBoxW(m_ownerProvider ? m_ownerProvider() : nullptr,
                                       message.c_str(),
                                       L"CloverPic",
                                       MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON1);
        switch (result) {
            case IDYES: return UnsavedChangesChoice::Save;
            case IDNO: return UnsavedChangesChoice::Discard;
            default: break;
        }
        return UnsavedChangesChoice::Cancel;
    }

private:
    std::function<HWND()> m_ownerProvider;

    bool ShowDialog(String& inOutPath,
                    const wchar_t* filter,
                    const wchar_t* defaultExtension,
                    DWORD flags,
                    bool saveDialog) {
        wchar_t buffer[MAX_PATH] = {0};
        if (!inOutPath.empty()) {
            wcsncpy_s(buffer, inOutPath.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_ownerProvider ? m_ownerProvider() : nullptr;
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = flags;
        ofn.lpstrDefExt = defaultExtension;

        const BOOL result = saveDialog ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
        if (!result) {
            return false;
        }

        inOutPath = buffer;
        return true;
    }
};

class WindowsFontCatalogProvider final : public IPlatformFontCatalogProvider {
public:
    PlatformFontCatalog LoadFontCatalog() override {
        PlatformFontCatalog catalog;
        catalog.systemUiFamily = GetSystemUiFamily();
        std::set<String> paths;
        AddFontDirectory(paths);
        AddRegistryFonts(HKEY_LOCAL_MACHINE, paths);
        AddRegistryFonts(HKEY_CURRENT_USER, paths);
        for (const auto& path : paths) {
            catalog.fonts.push_back({path});
            if (catalog.systemUiFontFileHint.empty()) {
                const String lowerPath = Lower(path);
                const String lowerFamily = Lower(catalog.systemUiFamily);
                if (!lowerFamily.empty() && lowerPath.find(lowerFamily) != String::npos) {
                    catalog.systemUiFontFileHint = path;
                }
            }
        }
        return catalog;
    }

private:
    String GetSystemUiFamily() const {
        NONCLIENTMETRICSW metrics = {};
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0) &&
            metrics.lfMessageFont.lfFaceName[0] != L'\0') {
            return metrics.lfMessageFont.lfFaceName;
        }
        return L"Segoe UI";
    }

    String GetFontsDirectory() const {
        wchar_t windowsDir[MAX_PATH] = {};
        if (GetWindowsDirectoryW(windowsDir, MAX_PATH) == 0) {
            return L"C:\\Windows\\Fonts";
        }
        return String(windowsDir) + L"\\Fonts";
    }

    void AddFontDirectory(std::set<String>& paths) const {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(GetFontsDirectory()))) {
                if (entry.is_regular_file() && IsFontFile(entry.path().wstring())) {
                    paths.insert(entry.path().wstring());
                }
            }
        } catch (...) {
        }
    }

    void AddRegistryFonts(HKEY root, std::set<String>& paths) const {
        HKEY key = nullptr;
        constexpr const wchar_t* subkey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
        if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return;

        const String fontsDir = GetFontsDirectory();
        for (DWORD index = 0;; ++index) {
            wchar_t name[512] = {};
            wchar_t data[1024] = {};
            DWORD nameSize = 512;
            DWORD dataSize = sizeof(data);
            DWORD type = 0;
            const LONG status = RegEnumValueW(key, index, name, &nameSize, nullptr, &type, reinterpret_cast<LPBYTE>(data), &dataSize);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
            String path = data;
            if (path.empty()) continue;
            if (path.find(L":\\") == String::npos && path.find(L"\\\\") != 0) {
                path = fontsDir + L"\\" + path;
            }
            if (IsFontFile(path)) paths.insert(path);
        }
        RegCloseKey(key);
    }

    bool IsFontFile(const String& path) const {
        const String lower = Lower(path);
        return lower.ends_with(L".ttf") || lower.ends_with(L".ttc");
    }

    String Lower(String value) const {
        for (auto& ch : value) ch = static_cast<wchar_t>(std::towlower(ch));
        return value;
    }
};

class WindowsColorProfileProvider final : public IColorProfileProvider {
public:
    std::vector<ColorProfileInfo> EnumerateColorProfiles() override {
        std::vector<ColorProfileInfo> profiles;
        std::set<String> profilePaths;
        const String currentPath = GetCurrentDisplayProfile().path;
        AddDisplayIcmProfiles(profilePaths);
        const String colorDir = GetColorDirectory();
        try {
            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(colorDir))) {
                if (!entry.is_regular_file()) continue;
                const String path = entry.path().wstring();
                const String lower = Lower(path);
                if (!lower.ends_with(L".icc") && !lower.ends_with(L".icm")) continue;
                profilePaths.insert(path);
            }
        } catch (...) {
        }

        for (const auto& path : profilePaths) {
            String resolvedPath = path;
            if (resolvedPath.find(L":\\") == String::npos && resolvedPath.find(L"\\\\") != 0) {
                resolvedPath = colorDir + L"\\" + resolvedPath;
            }
            ColorProfileInfo info;
            info.path = resolvedPath;
            info.id = resolvedPath;
            const size_t slash = resolvedPath.find_last_of(L"\\/");
            info.displayName = slash == String::npos ? resolvedPath : resolvedPath.substr(slash + 1);
            info.isCurrentDisplayProfile = !currentPath.empty() && Lower(currentPath) == Lower(resolvedPath);
            info.isDefault = info.isCurrentDisplayProfile;
            profiles.push_back(info);
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
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            DWORD profileSize = 0;
            if (!GetICMProfileW(hdc, &profileSize, nullptr) && profileSize > 0) {
                std::vector<wchar_t> profile(profileSize + 1, L'\0');
                if (GetICMProfileW(hdc, &profileSize, profile.data())) {
                    info.path = profile.data();
                    info.id = info.path;
                    const size_t slash = info.path.find_last_of(L"\\/");
                    info.displayName = slash == String::npos ? info.path : info.path.substr(slash + 1);
                }
            }
            ReleaseDC(nullptr, hdc);
        }
        info.isCurrentDisplayProfile = true;
        info.isDefault = true;
        if (info.displayName.empty()) info.displayName = L"sRGB fallback";
        return info;
    }

    bool ReadProfileBytes(const String& path, std::vector<uint8_t>& outBytes) override {
        outBytes.clear();
        if (path.empty()) return false;
        WindowsFileSystem fileSystem;
        return fileSystem.ReadFileBytes(path, outBytes);
    }

private:
    static int CALLBACK EnumIcmProfileProc(LPWSTR profileName, LPARAM userData) {
        if (!profileName || !userData) return TRUE;
        auto* paths = reinterpret_cast<std::set<String>*>(userData);
        String path = profileName;
        if (!path.empty()) {
            paths->insert(path);
        }
        return TRUE;
    }

    void AddDisplayIcmProfiles(std::set<String>& paths) const {
        HDC hdc = GetDC(nullptr);
        if (!hdc) return;
        EnumICMProfilesW(hdc, EnumIcmProfileProc, reinterpret_cast<LPARAM>(&paths));
        ReleaseDC(nullptr, hdc);
    }

    String GetColorDirectory() const {
        wchar_t systemDir[MAX_PATH] = {};
        if (GetSystemDirectoryW(systemDir, MAX_PATH) == 0) {
            return L"C:\\Windows\\System32\\spool\\drivers\\color";
        }
        return String(systemDir) + L"\\spool\\drivers\\color";
    }

    String Lower(String value) const {
        for (auto& ch : value) ch = static_cast<wchar_t>(std::towlower(ch));
        return value;
    }
};

class WindowsAppSettingsStore final : public IAppSettingsStore {
public:
    bool LoadSettingsBytes(std::vector<uint8_t>& outBytes) override {
        WindowsFileSystem fileSystem;
        return fileSystem.ReadFileBytes(GetStoragePath(), outBytes);
    }

    bool SaveSettingsBytes(const std::vector<uint8_t>& bytes) override {
        WindowsFileSystem fileSystem;
        return fileSystem.WriteFileBytes(GetStoragePath(), bytes);
    }

private:
    String GetStoragePath() const {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
            String result = path;
            result += L"\\CloverPic";
            CreateDirectoryW(result.c_str(), nullptr);
            result += L"\\settings.bin";
            return result;
        }
        return L"settings.bin";
    }
};

} // namespace

void RegisterWindowsCoreServices(std::function<HWND()> ownerProvider) {
    PlatformServices services;
    services.memoryInfoProvider = MakeRef<WindowsMemoryInfoProvider>();
    services.displayInfoProvider = MakeRef<WindowsDisplayInfoProvider>();
    services.recentFilesStore = MakeRef<WindowsRecentFilesStore>();
    services.fileSystem = MakeRef<WindowsFileSystem>();
    services.imageCodec = MakeRef<WindowsImageCodec>();
    services.fileDialogService = MakeRef<WindowsFileDialogService>(std::move(ownerProvider));
    services.fontCatalogProvider = MakeRef<WindowsFontCatalogProvider>();
    services.colorProfileProvider = MakeRef<WindowsColorProfileProvider>();
    services.appSettingsStore = MakeRef<WindowsAppSettingsStore>();
    CoreServices::InstallPlatformServices(std::move(services));
}

} // namespace CloverPic
