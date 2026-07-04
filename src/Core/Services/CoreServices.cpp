#include "Core/Services/CoreServices.h"

namespace CloverPic {

namespace {
Ref<IMemoryInfoProvider> g_memoryInfoProvider;
Ref<IDisplayInfoProvider> g_displayInfoProvider;
Ref<IRecentFilesStore> g_recentFilesStore;
Ref<IPlatformFileSystem> g_fileSystem;
Ref<IImageCodec> g_imageCodec;
Ref<IFileDialogService> g_fileDialogService;
Ref<IPlatformFontCatalogProvider> g_fontCatalogProvider;
Ref<IColorProfileProvider> g_colorProfileProvider;
Ref<IAppSettingsStore> g_appSettingsStore;
}

void CoreServices::InstallPlatformServices(PlatformServices services) {
    g_memoryInfoProvider = std::move(services.memoryInfoProvider);
    g_displayInfoProvider = std::move(services.displayInfoProvider);
    g_recentFilesStore = std::move(services.recentFilesStore);
    g_fileSystem = std::move(services.fileSystem);
    g_imageCodec = std::move(services.imageCodec);
    g_fileDialogService = std::move(services.fileDialogService);
    g_fontCatalogProvider = std::move(services.fontCatalogProvider);
    g_colorProfileProvider = std::move(services.colorProfileProvider);
    g_appSettingsStore = std::move(services.appSettingsStore);
}

IMemoryInfoProvider* CoreServices::GetMemoryInfoProvider() {
    return g_memoryInfoProvider.get();
}

IDisplayInfoProvider* CoreServices::GetDisplayInfoProvider() {
    return g_displayInfoProvider.get();
}

IRecentFilesStore* CoreServices::GetRecentFilesStore() {
    return g_recentFilesStore.get();
}

IPlatformFileSystem* CoreServices::GetFileSystem() {
    return g_fileSystem.get();
}

IImageCodec* CoreServices::GetImageCodec() {
    return g_imageCodec.get();
}

IFileDialogService* CoreServices::GetFileDialogService() {
    return g_fileDialogService.get();
}

IPlatformFontCatalogProvider* CoreServices::GetFontCatalogProvider() {
    return g_fontCatalogProvider.get();
}

IColorProfileProvider* CoreServices::GetColorProfileProvider() {
    return g_colorProfileProvider.get();
}

IAppSettingsStore* CoreServices::GetAppSettingsStore() {
    return g_appSettingsStore.get();
}

} // namespace CloverPic
