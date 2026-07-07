#include "Core/App/ProjectManagerRuntime.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/Services/CoreServices.h"
#include "Core/Text/CoreTextEngine.h"
#include "Core/UI/ProjectManager/Components/ProjectManagerComponentBuilder.h"
#include "Core/UI/ProjectManager/Interaction/ProjectManagerKeyInputController.h"
#include "Core/UI/ProjectManager/Interaction/ProjectManagerPointerInputController.h"
#include "Core/UI/ProjectManager/Persistence/ProjectManagerSettingsPersistence.h"
#include "Core/UI/ProjectManager/ProjectManagerSceneBuilder.h"
#include "Core/UI/ProjectManager/Render/ProjectManagerRenderer.h"
#include <algorithm>

namespace CloverPic::Core {

bool ProjectManagerRuntime::Initialize() {
    if (auto* fonts = CoreServices::GetFontCatalogProvider()) {
        CoreText::CoreTextEngine::Get().Initialize(fonts->LoadFontCatalog());
    }
    LoadColorProfiles();
    BuildScene();
    MarkDirty(FullRect());
    return true;
}

void ProjectManagerRuntime::Resize(uint32_t width, uint32_t height, float dpiScale) {
    m_viewport = Size(static_cast<int32_t>(width), static_cast<int32_t>(height));
    m_dpiScale = dpiScale;
    m_frame.Resize(width, height);
    BuildScene();
    m_scheduler.Reset();
    MarkDirty(FullRect());
}

const RgbaFrame& ProjectManagerRuntime::Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) {
    if (m_frame.IsEmpty()) {
        outDirtyRects.clear();
        return m_frame;
    }
    Presentation::SoftRenderer renderer(m_frame);
    ProjectManagerRenderer::Render(renderer, m_viewport, m_uiState, m_scene);
    outDirtyRects = m_scheduler.ConsumeDirtyRects(nowMs, FullRect());
    if (outDirtyRects.empty()) outDirtyRects.push_back(FullRect());
    return m_frame;
}

void ProjectManagerRuntime::HandlePointer(const Input::PointerEvent& event) {
    if (m_frame.IsEmpty()) return;
    ProjectManagerPointerInputContext context{ &m_scene, &m_uiState, FullRect() };
    const ProjectManagerPointerInputCallbacks callbacks{
        [this](AppCommand command, uint64_t userData, const String& payload) { DispatchCommand(command, userData, payload); },
        [this]() { BuildScene(); },
        [this](const Rect& rect) { MarkDirty(rect); }
    };
    ProjectManagerPointerInputController::HandlePointer(event, context, callbacks);
}

void ProjectManagerRuntime::HandleKey(const Input::KeyEvent& event) {
    ProjectManagerKeyInputContext context{ &m_scene, &m_uiState, &m_wantsQuit, FullRect() };
    const ProjectManagerKeyInputCallbacks callbacks{
        [this]() { BuildScene(); },
        [this](const Rect& rect) { MarkDirty(rect); },
        [this]() { OnCloseModal(); },
        [this]() { OnOpenProject(); },
        [this]() { StartNewCanvas(1600, 1000); }
    };
    ProjectManagerKeyInputController::HandleKey(event, context, callbacks);
}

void ProjectManagerRuntime::HandleWheel(int, const Point&) {}

bool ProjectManagerRuntime::NeedsFrame(uint64_t nowMs) const {
    return m_scheduler.HasPendingFrame(nowMs);
}

bool ProjectManagerRuntime::IsWindowDragRegion(const Point& position) {
    if (position.y <= 10) return true;
    return m_scene.HitTest(position) == nullptr;
}

WorkspaceLaunchRequest ProjectManagerRuntime::ConsumeLaunchRequest() {
    WorkspaceLaunchRequest request = m_launchRequest;
    m_launchRequest = {};
    return request;
}

void ProjectManagerRuntime::RefreshRecentFiles() {
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::ShowSettingsPage() {
    m_uiState.page = ProjectManagerPage::Settings;
    m_uiState.activeField = ProjectManagerActiveField::None;
    m_uiState.settingsOpenedFromWorkspace = true;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::BuildScene() {
    if (m_uiState.page == ProjectManagerPage::Start) {
        ProjectManagerSceneBuilder::BuildHomeScene(m_viewport, m_session, m_uiState.searchQuery, m_scene);
    } else {
        ProjectManagerComponentBuilder::Build(m_viewport, m_uiState, m_scene);
    }
}

void ProjectManagerRuntime::MarkDirty(const Rect& rect) {
    m_scheduler.Invalidate(rect);
}

void ProjectManagerRuntime::DispatchCommand(AppCommand command, uint64_t userData, const String& payload) {
    m_commands.Dispatch(*this, command, userData, payload);
}

void ProjectManagerRuntime::StartNewCanvas(uint32_t width, uint32_t height) {
    m_launchRequest.kind = WorkspaceLaunchKind::NewCanvas;
    m_launchRequest.width = width;
    m_launchRequest.height = height;
    m_launchRequest.dpi = static_cast<float>(m_uiState.formDpi);
    m_launchRequest.transparent = m_uiState.formTransparent;
    m_launchRequest.initialLayerType = m_uiState.formTransparent ? LayerType::Transparent : LayerType::Color;
    m_launchRequest.backgroundColor = m_uiState.formBackground;
    if (m_uiState.selectedRgbProfile < m_uiState.profiles.size()) {
        m_launchRequest.rgbProfilePath = m_uiState.profiles[m_uiState.selectedRgbProfile].path;
        if (auto* provider = CoreServices::GetColorProfileProvider()) {
            provider->ReadProfileBytes(m_launchRequest.rgbProfilePath, m_launchRequest.rgbProfileBytes);
        }
    }
    if (m_uiState.selectedCmykProfile < m_uiState.profiles.size()) {
        m_launchRequest.cmykProfilePath = m_uiState.profiles[m_uiState.selectedCmykProfile].path;
        if (auto* provider = CoreServices::GetColorProfileProvider()) {
            provider->ReadProfileBytes(m_launchRequest.cmykProfilePath, m_launchRequest.cmykProfileBytes);
        }
    }
}

void ProjectManagerRuntime::OpenProjectPath(const String& path) {
    if (path.empty()) return;
    m_launchRequest.kind = WorkspaceLaunchKind::OpenProject;
    m_launchRequest.projectPath = path;
}

void ProjectManagerRuntime::LoadColorProfiles() {
    m_uiState.profiles.clear();
    if (auto* provider = CoreServices::GetColorProfileProvider()) {
        m_uiState.profiles = provider->EnumerateColorProfiles();
    }
    if (m_uiState.profiles.empty()) {
        ColorProfileInfo fallback;
        fallback.id = L"srgb";
        fallback.displayName = L"sRGB fallback";
        fallback.isDefault = true;
        m_uiState.profiles.push_back(fallback);
    }
    m_uiState.selectedRgbProfile = 0;
    m_uiState.selectedCmykProfile = 0;
    for (size_t i = 0; i < m_uiState.profiles.size(); ++i) {
        if (m_uiState.profiles[i].isDefault || m_uiState.profiles[i].isCurrentDisplayProfile) {
            m_uiState.selectedRgbProfile = i;
            break;
        }
    }

    auto* store = CoreServices::GetAppSettingsStore();
    if (!store) return;
    std::vector<uint8_t> bytes;
    if (!store->LoadSettingsBytes(bytes) || bytes.empty()) return;
    ProjectManagerSettingsPersistence::LoadSelectedProfiles(bytes, m_uiState, m_uiState.settingsStatus);
}

void ProjectManagerRuntime::SaveSettingsDraft(bool persist) {
    m_uiState.settingsStatus = persist ? L"Settings saved." : L"Settings applied for this session.";
    if (!persist) return;
    if (auto* store = CoreServices::GetAppSettingsStore()) {
        std::vector<uint8_t> bytes;
        ProjectManagerSettingsPersistence::SaveSelectedProfiles(bytes, m_uiState);
        store->SaveSettingsBytes(bytes);
    }
}

String ProjectManagerRuntime::ProfileLabel(size_t index) const {
    if (index >= m_uiState.profiles.size()) return L"sRGB fallback";
    const auto& profile = m_uiState.profiles[index];
    if (!profile.displayName.empty()) return profile.displayName;
    if (!profile.path.empty()) {
        const size_t slash = profile.path.find_last_of(L"\\/");
        return slash == String::npos ? profile.path : profile.path.substr(slash + 1);
    }
    return L"sRGB fallback";
}

String ProjectManagerRuntime::ActiveFieldPayload(ProjectManagerActiveField field) const {
    return ProjectManagerUiState::FieldPayload(field);
}

void ProjectManagerRuntime::OnShowNewCanvasModal() {
    m_uiState.page = ProjectManagerPage::NewImage;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnShowSettingsModal() {
    m_uiState.page = ProjectManagerPage::Settings;
    m_uiState.settingsOpenedFromWorkspace = false;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnCreateCanvasFromForm() {
    StartNewCanvas(std::clamp<uint32_t>(m_uiState.formWidth, 16, 65535),
                   std::clamp<uint32_t>(m_uiState.formHeight, 16, 65535));
    m_uiState.page = ProjectManagerPage::Start;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSwapCanvasOrientation() {
    std::swap(m_uiState.formWidth, m_uiState.formHeight);
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnToggleCanvasTransparency() {
    m_uiState.formTransparent = !m_uiState.formTransparent;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSelectRgbProfile(size_t index) {
    if (index < m_uiState.profiles.size()) m_uiState.selectedRgbProfile = index;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSelectCmykProfile(size_t index) {
    if (index < m_uiState.profiles.size()) m_uiState.selectedCmykProfile = index;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSetSettingsCategory(size_t index) {
    m_uiState.settingsCategory = static_cast<int>(std::clamp<size_t>(index, 0, 5));
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnApplySettings() {
    SaveSettingsDraft(false);
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSaveSettings() {
    SaveSettingsDraft(true);
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnCreateCanvasPreset(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        OnSwapCanvasOrientation();
        return;
    }
    m_uiState.formWidth = width;
    m_uiState.formHeight = height;
    StartNewCanvas(width, height);
}

void ProjectManagerRuntime::OnCloseModal() {
    if (m_uiState.page == ProjectManagerPage::Settings && m_uiState.settingsOpenedFromWorkspace) {
        m_uiState.settingsOpenedFromWorkspace = false;
        m_wantsQuit = true;
        return;
    }
    m_uiState.page = ProjectManagerPage::Start;
    m_uiState.activeField = ProjectManagerActiveField::None;
    m_uiState.settingsOpenedFromWorkspace = false;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnOpenProject() {
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) return;
    String path;
    if (dialogs->PickOpenProjectPath(path)) {
        OpenProjectPath(path);
    }
}

void ProjectManagerRuntime::OnOpenRecentProject(const String& path) {
    OpenProjectPath(path);
}

void ProjectManagerRuntime::OnFocusHomeSearch() {
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnQuit() {
    m_wantsQuit = true;
}

} // namespace CloverPic::Core
