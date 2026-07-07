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
    m_hasUnsavedChanges = false;
}

bool EditorSession::OpenProject(const String& filePath, LayerManager* layerManager) {
    ClearError();
    if (filePath.empty()) {
        return Fail(L"Project path is empty.");
    }
    if (!layerManager) {
        return Fail(L"Layer manager is missing.");
    }

    auto loadedProject = ProjectSerializer::LoadProject(filePath, layerManager);
    if (!loadedProject) {
        return Fail(L"Failed to open the project file.");
    }

    m_project = std::move(loadedProject);
    m_currentFilePath = filePath;
    m_hasLoadedLayerState = true;
    m_hasUnsavedChanges = false;
    RememberRecentFile(filePath);
    return true;
}

bool EditorSession::SaveProject(LayerManager* layerManager) {
    if (m_currentFilePath.empty()) {
        return Fail(L"Current project does not have a save path yet.");
    }
    return SaveProjectAs(m_currentFilePath, layerManager);
}

bool EditorSession::SaveProjectAs(const String& filePath, LayerManager* layerManager) {
    ClearError();
    if (!m_project) {
        return Fail(L"There is no project to save.");
    }
    if (!layerManager) {
        return Fail(L"Layer manager is missing.");
    }
    if (filePath.empty()) {
        return Fail(L"Save path is empty.");
    }

    m_project->GetMetadata().modifiedAt = std::chrono::system_clock::now();
    if (!ProjectSerializer::SaveProject(filePath, m_project.get(), layerManager)) {
        return Fail(L"Failed to save the project.");
    }

    m_currentFilePath = filePath;
    m_hasUnsavedChanges = false;
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
