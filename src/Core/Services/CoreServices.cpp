#include "Core/Services/CoreServices.h"

namespace CloverPic {

namespace {
Ref<IMemoryInfoProvider> g_memoryInfoProvider;
Ref<IDisplayInfoProvider> g_displayInfoProvider;
Ref<IRecentFilesStore> g_recentFilesStore;
Ref<IPlatformFileSystem> g_fileSystem;
Ref<IImageEncoder> g_imageEncoder;
Ref<IFileDialogService> g_fileDialogService;
Ref<IPlatformFontCatalogProvider> g_fontCatalogProvider;
}

void CoreServices::InstallPlatformServices(PlatformServices services) {
    g_memoryInfoProvider = std::move(services.memoryInfoProvider);
    g_displayInfoProvider = std::move(services.displayInfoProvider);
    g_recentFilesStore = std::move(services.recentFilesStore);
    g_fileSystem = std::move(services.fileSystem);
    g_imageEncoder = std::move(services.imageEncoder);
    g_fileDialogService = std::move(services.fileDialogService);
    g_fontCatalogProvider = std::move(services.fontCatalogProvider);
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

IImageEncoder* CoreServices::GetImageEncoder() {
    return g_imageEncoder.get();
}

IFileDialogService* CoreServices::GetFileDialogService() {
    return g_fileDialogService.get();
}

IPlatformFontCatalogProvider* CoreServices::GetFontCatalogProvider() {
    return g_fontCatalogProvider.get();
}

} // namespace CloverPic
