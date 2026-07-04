#pragma once

#include "Core/Project.h"
#include "Utils/Types.h"
#include <vector>

namespace CloverPic {

class LayerManager;

class EditorSession {
public:
    void AttachProject(Ref<Project> project, const String& currentFilePath = L"");

    bool OpenProject(const String& filePath, LayerManager* layerManager);
    bool SaveProject(LayerManager* layerManager);
    bool SaveProjectAs(const String& filePath, LayerManager* layerManager);
    bool ExportPNG(const String& filePath, LayerManager* layerManager) const;

    std::vector<String> GetRecentFiles() const;

    const Ref<Project>& GetProject() const { return m_project; }
    const String& GetCurrentFilePath() const { return m_currentFilePath; }
    const String& GetLastError() const { return m_lastError; }
    bool HasProject() const { return static_cast<bool>(m_project); }
    bool HasLoadedLayerState() const { return m_hasLoadedLayerState; }

private:
    bool Fail(const String& message);
    void ClearError();
    void RememberRecentFile(const String& filePath) const;

    Ref<Project> m_project;
    String m_currentFilePath;
    String m_lastError;
    bool m_hasLoadedLayerState = false;

    static constexpr size_t MaxRecentFiles = 10;
};

} // namespace CloverPic
