#include "Core/EditorSession.h"
#include "Core/LayerManager.h"
#include "Core/ProjectIO.h"
#include "Core/Services/CoreServices.h"
#include <algorithm>
#include <chrono>

namespace CloverPic {

void EditorSession::AttachProject(Ref<Project> project, const String& currentFilePath) {
    ClearError();
    m_project = std::move(project);
    m_currentFilePath = currentFilePath;
    m_hasLoadedLayerState = false;
}

bool EditorSession::OpenProject(const String& filePath, LayerManager* layerManager) {
    ClearError();
    if (filePath.empty()) {
        return Fail(L"项目路径为空");
    }
    if (!layerManager) {
        return Fail(L"缺少图层管理器");
    }

    auto loadedProject = ProjectSerializer::LoadProject(filePath, layerManager);
    if (!loadedProject) {
        return Fail(L"无法打开项目文件");
    }

    m_project = std::move(loadedProject);
    m_currentFilePath = filePath;
    m_hasLoadedLayerState = true;
    RememberRecentFile(filePath);
    return true;
}

bool EditorSession::SaveProject(LayerManager* layerManager) {
    if (m_currentFilePath.empty()) {
        return Fail(L"当前项目还没有保存路径");
    }
    return SaveProjectAs(m_currentFilePath, layerManager);
}

bool EditorSession::SaveProjectAs(const String& filePath, LayerManager* layerManager) {
    ClearError();
    if (!m_project) {
        return Fail(L"当前没有可保存的项目");
    }
    if (!layerManager) {
        return Fail(L"缺少图层管理器");
    }
    if (filePath.empty()) {
        return Fail(L"保存路径为空");
    }

    m_project->GetMetadata().modifiedAt = std::chrono::system_clock::now();
    if (!ProjectSerializer::SaveProject(filePath, m_project.get(), layerManager)) {
        return Fail(L"保存项目失败");
    }

    m_currentFilePath = filePath;
    RememberRecentFile(filePath);
    return true;
}

bool EditorSession::ExportPNG(const String& filePath, LayerManager* layerManager) const {
    if (!layerManager || filePath.empty()) {
        return false;
    }
    return ProjectSerializer::ExportPNG(filePath, layerManager);
}

std::vector<String> EditorSession::GetRecentFiles() const {
    if (auto* store = CoreServices::GetRecentFilesStore()) {
        return store->LoadRecentFiles();
    }
    return {};
}

bool EditorSession::Fail(const String& message) {
    m_lastError = message;
    return false;
}

void EditorSession::ClearError() {
    m_lastError.clear();
}

void EditorSession::RememberRecentFile(const String& filePath) const {
    auto* store = CoreServices::GetRecentFilesStore();
    if (!store || filePath.empty()) {
        return;
    }

    auto files = store->LoadRecentFiles();
    files.erase(std::remove(files.begin(), files.end(), filePath), files.end());
    files.insert(files.begin(), filePath);
    if (files.size() > MaxRecentFiles) {
        files.resize(MaxRecentFiles);
    }
    store->SaveRecentFiles(files);
}

} // namespace CloverPic
