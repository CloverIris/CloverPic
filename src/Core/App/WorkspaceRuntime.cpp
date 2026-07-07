#include "Core/App/WorkspaceRuntime.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/App/WorkspaceActionController.h"
#include "Core/UI/Workspace/Components/WorkspaceComponentBuilder.h"
#include "Core/UI/Workspace/Interaction/WorkspaceKeyInputController.h"
#include "Core/UI/Workspace/WorkspaceLayout.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalBuilder.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalRenderer.h"
#include "Core/UI/Workspace/Modal/WorkspaceModalState.h"
#include "Core/UI/Workspace/Render/WorkspaceNodeRenderer.h"
#include "Core/UI/Workspace/Interaction/WorkspacePointerInputController.h"
#include "Core/UI/Workspace/Persistence/WorkspaceUiSettingsPersistence.h"
#include "Core/Layers/BlendMode.h"
#include "Core/Layers/Layer.h"
#include "Core/Presentation/IconPainter.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/Services/CoreServices.h"
#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

namespace CloverPic::Core {

namespace {

constexpr int MenuBarH = WorkspaceLayout::MenuBarH;
constexpr int TopBarH = WorkspaceLayout::TopBarH;
constexpr Presentation::UiNodeId CanvasAnimationRegionId = 1;

String ToolName(ToolType tool) {
    switch (tool) {
        case ToolType::Brush: return L"BRUSH";
        case ToolType::Eraser: return L"ERASER";
        case ToolType::Eyedropper: return L"PICK";
        case ToolType::Fill: return L"FILL";
        case ToolType::Move: return L"MOVE";
        case ToolType::RectSelect: return L"SELECT";
        case ToolType::Text: return L"TEXT";
        case ToolType::Shape: return L"SHAPE";
        default: return L"TOOL";
    }
}

String TipName(Render::BrushTipType tip) {
    switch (tip) {
        case Render::BrushTipType::RoundHard: return L"HARD";
        case Render::BrushTipType::RoundSoft: return L"SOFT";
        case Render::BrushTipType::Flat: return L"FLAT";
        case Render::BrushTipType::Bristle: return L"BRISTLE";
        case Render::BrushTipType::Texture: return L"TEXTURE";
    }
    return L"TIP";
}

String BlendName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return L"NORMAL";
        case BlendMode::Multiply: return L"MULTIPLY";
        case BlendMode::Screen: return L"SCREEN";
        case BlendMode::Overlay: return L"OVERLAY";
        case BlendMode::Add: return L"ADD";
        default: return L"BLEND";
    }
}

String SnapModeName(SnapModeId mode) {
    switch (mode) {
        case SnapModeId::Off: return L"SNAP OFF";
        case SnapModeId::Parallel: return L"SNAP PARALLEL";
        case SnapModeId::Crisscross: return L"SNAP CRISSCROSS";
        case SnapModeId::VanishingPoint: return L"SNAP VANISH";
        case SnapModeId::Radial: return L"SNAP RADIAL";
        case SnapModeId::Circle: return L"SNAP CIRCLE";
        case SnapModeId::Curve: return L"SNAP CURVE";
        case SnapModeId::CurvedLineEllipse: return L"SNAP ELLIPSE";
        default: return L"SNAP";
    }
}

} // namespace

WorkspaceRuntime::WorkspaceRuntime(WorkspaceLaunchRequest launchRequest) : m_launchRequest(std::move(launchRequest)) {
    m_editor.Bind(m_canvas);
}

bool WorkspaceRuntime::Initialize() {
    if (auto* fonts = CoreServices::GetFontCatalogProvider()) {
        CoreText::CoreTextEngine::Get().Initialize(fonts->LoadFontCatalog());
    }

    m_uiState.Reset(m_viewport);
    RecomputeLayout();

    if (m_launchRequest.kind != WorkspaceLaunchKind::None) {
        OpenLaunchRequest(m_launchRequest);
    } else {
        m_status = L"NO PROJECT";
    }
    LoadWorkspaceUiSettings();
    RecomputeLayout();
    if (m_canvas.HasProject()) {
        m_canvas.ResizeViewport(CanvasRect());
    }

    BuildScene();
    MarkDirty(FullRect());
    return true;
}

bool WorkspaceRuntime::OpenLaunchRequest(const WorkspaceLaunchRequest& launchRequest) {
    if (launchRequest.kind == WorkspaceLaunchKind::OpenProject) {
        if (m_session.OpenProject(launchRequest.projectPath, m_canvas.GetLayerManager())) {
            m_canvas.AttachLoadedProject(m_session.GetProject());
            RecomputeLayout();
            m_canvas.ResizeViewport(CanvasRect());
            m_status = L"OPENED";
            m_uiState.statusText = m_status;
            return true;
        }
        m_status = L"OPEN FAILED";
        m_uiState.statusText = m_status;
        return false;
    }

    if (launchRequest.kind == WorkspaceLaunchKind::NewCanvas) {
        m_canvas.NewCanvas(launchRequest.width, launchRequest.height, launchRequest.dpi, launchRequest.transparent,
                           launchRequest.initialLayerType, launchRequest.backgroundColor,
                           launchRequest.rgbProfilePath, launchRequest.cmykProfilePath,
                           launchRequest.rgbProfileBytes, launchRequest.cmykProfileBytes);
        m_session.AttachProject(m_canvas.GetProject());
        m_session.MarkDirty();
        RecomputeLayout();
        m_canvas.ResizeViewport(CanvasRect());
        m_status = L"NEW CANVAS";
        m_uiState.statusText = m_status;
        return true;
    }

    return false;
}

void WorkspaceRuntime::AcceptWorkspaceLaunchRequest(WorkspaceLaunchRequest launchRequest) {
    m_launchRequest = std::move(launchRequest);
    OpenLaunchRequest(m_launchRequest);
    BuildScene();
    MarkDirty(FullRect());
}

bool WorkspaceRuntime::ConsumeProgramManagerRequest() {
    const bool value = m_wantsProgramManager;
    m_wantsProgramManager = false;
    return value;
}

bool WorkspaceRuntime::ConsumeProgramManagerSettingsRequest() {
    const bool value = m_wantsProgramManagerSettings;
    m_wantsProgramManagerSettings = false;
    return value;
}

void WorkspaceRuntime::Resize(uint32_t width, uint32_t height, float dpiScale) {
    m_viewport = Size(static_cast<int32_t>(width), static_cast<int32_t>(height));
    m_dpiScale = dpiScale;
    m_frame.Resize(width, height);
    if (m_uiState.panels.empty()) {
        m_uiState.Reset(m_viewport);
    }
    RecomputeLayout();
    m_canvas.ResizeViewport(CanvasRect());
    BuildScene();
    m_scheduler.Reset();
    MarkDirty(FullRect());
}

const RgbaFrame& WorkspaceRuntime::Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) {
    if (m_frame.IsEmpty()) {
        outDirtyRects.clear();
        return m_frame;
    }
    Presentation::SoftRenderer renderer(m_frame);
    RenderWorkspace(renderer);
    if (m_modals.HasModal()) {
        RenderModal(renderer);
    }
    (void)m_scheduler.ConsumeDirtyRects(nowMs, FullRect());
    outDirtyRects.clear();
    outDirtyRects.push_back(FullRect());
    return m_frame;
}

void WorkspaceRuntime::HandlePointer(const Input::PointerEvent& event) {
    if (m_frame.IsEmpty()) {
        return;
    }

    WorkspacePointerInputContext context{
        &m_scene,
        &m_uiState,
        &m_layout,
        &m_interaction,
        &m_editor,
        &m_scheduler,
        m_modals.Current(),
        &m_openMenu,
        &m_canvasSizeState,
        CanvasRect(),
        FullRect(),
        CanvasAnimationRegionId
    };
    const WorkspacePointerInputCallbacks callbacks{
        [this](AppCommand command, uint64_t userData, const String& payload) { DispatchCommand(command, userData, payload); },
        [this](const Rect& rect) { MarkDirty(rect); },
        [this]() { MarkProjectDirty(); },
        [this]() { TouchWorkspace(); },
        [this](const String& status) {
            m_status = status;
            m_uiState.statusText = status;
        },
        [this]() { SaveWorkspaceUiSettings(); }
    };
    WorkspacePointerInputController::HandlePointer(event, context, callbacks);
}

void WorkspaceRuntime::HandleKey(const Input::KeyEvent& event) {
    WorkspaceKeyInputContext context{
        m_modals.Current(),
        &m_canvasSizeState,
        &m_openMenu,
        &m_wantsClose,
        &m_editor
    };
    const WorkspaceKeyInputCallbacks callbacks{
        [this]() { TouchWorkspace(); },
        [this]() { OnCloseModal(); },
        [this]() { OnCreateCanvasFromForm(); },
        [this](bool saveAs) { SaveProject(saveAs); },
        [this]() { OnUndo(); },
        [this]() { OnRedo(); },
        [this]() { OnSelectAll(); },
        [this]() { OnClearSelection(); },
        [this]() { OnInvertSelection(); },
        [this]() { OnFitCanvas(); },
        [this](uint16_t zoomPercent) { OnSetZoomPreset(zoomPercent); },
        [this]() { OnShowSettingsModal(); },
        [this]() { OnZoomIn(); },
        [this]() { OnZoomOut(); },
        [this](ToolType tool) { SwitchTool(tool); },
        [this](const String& status) { SetStatus(status); }
    };
    if (!WorkspaceKeyInputController::HandleKeyDown(event, context, callbacks)) {
        return;
    }
    m_uiState.openMenu = m_openMenu;
}

void WorkspaceRuntime::HandleWheel(int delta, const Point& position) {
    if (!m_modals.HasModal() && CanvasRect().Contains(position)) {
        m_canvas.HandleWheel(delta, position, CanvasRect());
        MarkDirty(CanvasRect());
    }
}

bool WorkspaceRuntime::NeedsFrame(uint64_t nowMs) const {
    return m_scheduler.HasPendingFrame(nowMs);
}

void WorkspaceRuntime::BuildScene() {
    RecomputeLayout();
    m_uiState.openMenu = m_openMenu;
    m_uiState.statusText = m_status;
    WorkspaceComponentBuilder::Build(m_viewport, m_editor, m_uiState, m_layout, m_scene);
    WorkspaceModalBuilder::Build(m_modals.Current(), m_viewport, FullRect(), m_editor, m_canvasSizeState, m_scene);
}

void WorkspaceRuntime::RecomputeLayout() {
    m_layout = WorkspaceDockLayout::Compute(m_viewport, m_uiState, m_uiState.IsPanelVisible(WorkspacePanelId::StatusBar));
}

void WorkspaceRuntime::LoadWorkspaceUiSettings() {
    if (m_uiState.panels.empty()) {
        m_uiState.Reset(m_viewport);
    }
    auto* store = CoreServices::GetAppSettingsStore();
    if (!store) {
        return;
    }
    std::vector<uint8_t> bytes;
    if (!store->LoadSettingsBytes(bytes) || bytes.empty()) {
        return;
    }
    WorkspaceUiSettingsPersistence::LoadFromBytes(bytes, m_uiState);
}

void WorkspaceRuntime::SaveWorkspaceUiSettings() {
    auto* store = CoreServices::GetAppSettingsStore();
    if (!store) {
        return;
    }
    std::vector<uint8_t> bytes;
    if (!store->LoadSettingsBytes(bytes) || bytes.empty()) {
        bytes.resize(sizeof(uint32_t) * 2, 0);
    }
    WorkspaceUiSettingsPersistence::SaveToBytes(bytes, m_uiState);
    store->SaveSettingsBytes(bytes);
}

void WorkspaceRuntime::MarkDirty(const Rect& rect) {
    m_scheduler.Invalidate(rect);
}

Rect WorkspaceRuntime::CanvasRect() const { return m_layout.canvasRect; }

void WorkspaceRuntime::DispatchCommand(AppCommand command, uint64_t userData, const String& payload) {
    m_commands.Dispatch(*this, command, userData, payload);
}

void WorkspaceRuntime::RenderWorkspace(Presentation::SoftRenderer& renderer) {
    const WorkspaceNodeRenderContext nodeContext{
        m_scene, m_uiState, m_editor, m_modals.Current(), static_cast<uint64_t>(m_canvasSizeState.anchor)
    };
    renderer.Clear(Color(66, 63, 63));
    renderer.FillRect(m_layout.menuBarRect, Color(27, 29, 31));
    renderer.FillRect(m_layout.commandBarRect, Color(35, 37, 40));
    renderer.StrokeRect(Rect(0, 0, m_viewport.width, TopBarH), Color(75, 80, 86), 1);
    renderer.FillRect(m_layout.toolRailRect, Color(29, 31, 34));
    renderer.FillRect(m_layout.leftDockRect, Color(36, 38, 40));
    renderer.FillRect(m_layout.rightDockRect, Color(36, 38, 40));
    if (m_uiState.IsPanelVisible(WorkspacePanelId::StatusBar)) {
        renderer.FillRect(m_layout.statusBarRect, Color(28, 30, 32));
    }
    renderer.FillVerticalGradient(CanvasRect(), Color(82, 79, 79), Color(68, 65, 65));

    m_canvas.Render(renderer, CanvasRect());
    RenderWorkspacePanels(renderer);

    std::vector<const CoreUI::UiNode*> nodes;
    nodes.reserve(m_scene.Nodes().size());
    for (const auto& node : m_scene.Nodes()) {
        nodes.push_back(&node);
    }
    std::sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b) {
        if (a->zOrder != b->zOrder) return a->zOrder < b->zOrder;
        return a->id < b->id;
    });
    for (const auto* node : nodes) {
        if (node->type == CoreUI::UiNodeType::Panel && node->zOrder >= 100) {
            renderer.FillRect(node->bounds, Color(30, 32, 34));
            renderer.StrokeRect(node->bounds, Color(68, 72, 76), 1);
        } else if (node->type == CoreUI::UiNodeType::PanelHeader) {
            continue;
        } else {
            const bool activeTool = node->type == CoreUI::UiNodeType::ToolbarItem &&
                                    node->userData == static_cast<uint64_t>(m_canvas.GetTool());
            WorkspaceNodeRenderer::RenderNode(renderer, *node, nodeContext, activeTool);
        }
    }

    if (m_uiState.dockInsertion.visible) {
        renderer.FillRect(m_uiState.dockInsertion.rect, Color(0, 120, 215, 210));
        renderer.StrokeRect(m_uiState.dockInsertion.rect, Color(176, 225, 255), 1);
    }
    if (m_uiState.draggingPanel) {
        renderer.StrokeRect(m_uiState.draggedPreviewRect, Color(0, 145, 255, 235), 2);
        renderer.StrokeRect(Rect(m_uiState.draggedPreviewRect.left + 2, m_uiState.draggedPreviewRect.top + 2,
                                 m_uiState.draggedPreviewRect.right - 2, m_uiState.draggedPreviewRect.bottom - 2),
                            Color(218, 240, 255, 140), 1);
    }

    if (m_uiState.IsPanelVisible(WorkspacePanelId::StatusBar)) {
        std::wstringstream status;
        status << m_status << L" | " << ToolName(m_canvas.GetTool()) << L" | SIZE " << static_cast<int>(m_canvas.GetBrushSize())
               << L" | OP " << static_cast<int>(m_canvas.GetBrushOpacity() * 100)
               << L"% | " << TipName(m_canvas.GetBrushTip()) << L" | ZOOM " << static_cast<int>(m_canvas.GetZoom() * 100) << L"%";
        renderer.DrawText(8, m_viewport.height - 18, status.str(), Color(210, 216, 220), 2);
    }
}

void WorkspaceRuntime::RenderWorkspacePanels(Presentation::SoftRenderer& renderer) {
    const WorkspaceNodeRenderContext nodeContext{
        m_scene, m_uiState, m_editor, m_modals.Current(), static_cast<uint64_t>(m_canvasSizeState.anchor)
    };
    WorkspaceNodeRenderer::RenderPanelDecorations(renderer, m_layout, nodeContext);
}

void WorkspaceRuntime::RenderModal(Presentation::SoftRenderer& renderer) {
    const WorkspaceNodeRenderContext nodeContext{
        m_scene, m_uiState, m_editor, m_modals.Current(), static_cast<uint64_t>(m_canvasSizeState.anchor)
    };
    WorkspaceModalRenderer::Render(renderer, FullRect(), m_viewport, nodeContext, m_canvasSizeState);
}

bool WorkspaceRuntime::SaveCurrentProjectInteractive(bool saveAs) {
    return SaveProject(saveAs);
}

WorkspaceActionContext WorkspaceRuntime::BuildActionContext() {
    return WorkspaceActionContext{
        &m_session,
        &m_canvas,
        &m_wantsProgramManager,
        &m_wantsProgramManagerSettings
    };
}

WorkspaceActionCallbacks WorkspaceRuntime::BuildActionCallbacks() {
    return WorkspaceActionCallbacks{
        [this](const String& status) { SetStatus(status); },
        [this]() { TouchWorkspace(); },
        [this]() { m_canvas.ResizeViewport(CanvasRect()); }
    };
}

bool WorkspaceRuntime::SaveProject(bool saveAs) {
    auto context = BuildActionContext();
    return WorkspaceActionController::SaveProject(saveAs, context, BuildActionCallbacks());
}

void WorkspaceRuntime::ExportPng() {
    auto context = BuildActionContext();
    WorkspaceActionController::ExportPng(context, BuildActionCallbacks());
}

void WorkspaceRuntime::SwitchTool(ToolType tool) {
    m_canvas.SetTool(tool);
    SetStatus(ToolName(tool));
}

void WorkspaceRuntime::SetStatus(const String& status) {
    m_status = status;
    m_uiState.statusText = status;
    TouchWorkspace();
}

void WorkspaceRuntime::MarkProjectDirty() {
    m_session.MarkDirty();
}

bool WorkspaceRuntime::ConfirmAbandonUnsavedChanges(const String& actionLabel) {
    auto context = BuildActionContext();
    return WorkspaceActionController::ConfirmAbandonUnsavedChanges(actionLabel, context, BuildActionCallbacks());
}

void WorkspaceRuntime::TouchWorkspace() {
    RecomputeLayout();
    BuildScene();
    MarkDirty(FullRect());
}

void WorkspaceRuntime::OnGoHome() {
    auto context = BuildActionContext();
    WorkspaceActionController::RequestProgramManager(context, BuildActionCallbacks());
}
void WorkspaceRuntime::OnShowNewCanvasModal() {
    auto context = BuildActionContext();
    WorkspaceActionController::RequestProgramManager(context, BuildActionCallbacks());
}
void WorkspaceRuntime::OnShowSettingsModal() {
    auto context = BuildActionContext();
    WorkspaceActionController::RequestProgramManagerSettings(context, BuildActionCallbacks());
}
void WorkspaceRuntime::OnCreateCanvasFromForm() {
    if (m_modals.Current() != ModalKind::CanvasSize || !m_session.HasProject()) {
        return;
    }
    const uint32_t width = std::clamp<uint32_t>(m_canvasSizeState.width, 1, 65535);
    const uint32_t height = std::clamp<uint32_t>(m_canvasSizeState.height, 1, 65535);
    const uint32_t anchorIndex = static_cast<uint32_t>(m_canvasSizeState.anchor);
    const uint32_t anchorX = anchorIndex % 3;
    const uint32_t anchorY = anchorIndex / 3;
    if (m_canvas.ResizeCanvas(width, height, anchorX, anchorY)) {
        MarkProjectDirty();
        m_modals.Close();
        m_canvasSizeState.focusedField = WorkspaceCanvasSizeField::None;
        m_canvas.ResizeViewport(CanvasRect());
        SetStatus(L"CANVAS RESIZED");
    } else {
        SetStatus(L"CANVAS RESIZE FAILED");
    }
}
void WorkspaceRuntime::OnSwapCanvasOrientation() {
    if (m_modals.Current() != ModalKind::CanvasSize) {
        return;
    }
    std::swap(m_canvasSizeState.width, m_canvasSizeState.height);
    TouchWorkspace();
}
void WorkspaceRuntime::OnSetCanvasAnchor(uint32_t anchor) {
    m_canvasSizeState.anchor = static_cast<WorkspaceCanvasAnchor>(std::min<uint32_t>(anchor, 8));
    TouchWorkspace();
}
void WorkspaceRuntime::OnCreateCanvasPreset(uint32_t width, uint32_t height) {
    if (m_modals.Current() == ModalKind::CanvasSize) {
        m_canvasSizeState.width = std::clamp<uint32_t>(width, 1, 65535);
        m_canvasSizeState.height = std::clamp<uint32_t>(height, 1, 65535);
        TouchWorkspace();
        return;
    }
    m_canvas.NewCanvas(width, height, 350.0f, true);
    m_session.AttachProject(m_canvas.GetProject());
    m_session.MarkDirty();
    m_modals.Close();
    m_canvas.ResizeViewport(CanvasRect());
    SetStatus(L"NEW CANVAS");
}
void WorkspaceRuntime::OnCloseModal() {
    m_canvasSizeState.focusedField = WorkspaceCanvasSizeField::None;
    m_modals.Close();
    TouchWorkspace();
}
void WorkspaceRuntime::OnOpenProject() {
    auto context = BuildActionContext();
    WorkspaceActionController::OpenProjectInteractive(context, BuildActionCallbacks());
}

void WorkspaceRuntime::OnOpenRecentProject(const String& path) {
    if (path.empty()) return;
    auto context = BuildActionContext();
    if (!WorkspaceActionController::ConfirmAbandonUnsavedChanges(L"opening another project", context, BuildActionCallbacks())) {
        return;
    }
    WorkspaceActionController::OpenProjectPath(path, context, BuildActionCallbacks());
}
void WorkspaceRuntime::OnShowFiltersModal() { m_modals.Show(ModalKind::Filters); TouchWorkspace(); }
void WorkspaceRuntime::OnShowTextLayerModal() { m_modals.Show(ModalKind::TextLayer); TouchWorkspace(); }
void WorkspaceRuntime::OnSaveProject(bool saveAs) { SaveProject(saveAs); }
void WorkspaceRuntime::OnExportPng() { ExportPng(); }
void WorkspaceRuntime::OnUndo() { m_canvas.GetHistoryManager()->Undo(); MarkProjectDirty(); MarkDirty(CanvasRect()); SetStatus(L"UNDO"); }
void WorkspaceRuntime::OnRedo() { m_canvas.GetHistoryManager()->Redo(); MarkProjectDirty(); MarkDirty(CanvasRect()); SetStatus(L"REDO"); }
void WorkspaceRuntime::OnShowCanvasSizeModal() {
    if (!m_session.HasProject()) {
        SetStatus(L"NO PROJECT");
        return;
    }
    if (auto project = m_canvas.GetProject()) {
        m_canvasSizeState.width = project->GetCanvas().widthPx;
        m_canvasSizeState.height = project->GetCanvas().heightPx;
    }
    m_canvasSizeState.anchor = WorkspaceCanvasAnchor::Center;
    m_canvasSizeState.focusedField = WorkspaceCanvasSizeField::Width;
    m_modals.Show(ModalKind::CanvasSize);
    TouchWorkspace();
}
void WorkspaceRuntime::OnFlipLayerHorizontal() { m_canvas.FlipActiveLayerHorizontal(); MarkProjectDirty(); SetStatus(L"FLIP HORIZONTAL"); }
void WorkspaceRuntime::OnFlipLayerVertical() { m_canvas.FlipActiveLayerVertical(); MarkProjectDirty(); SetStatus(L"FLIP VERTICAL"); }
void WorkspaceRuntime::OnRotateLayerLeft() { m_canvas.RotateActiveLayerLeft(); MarkProjectDirty(); SetStatus(L"ROTATE LEFT"); }
void WorkspaceRuntime::OnRotateLayerRight() { m_canvas.RotateActiveLayerRight(); MarkProjectDirty(); SetStatus(L"ROTATE RIGHT"); }
void WorkspaceRuntime::OnSelectAll() { m_canvas.SelectAll(); SetStatus(L"SELECT ALL"); }
void WorkspaceRuntime::OnClearSelection() { m_canvas.ClearSelection(); SetStatus(L"SELECTION CLEARED"); }
void WorkspaceRuntime::OnInvertSelection() { m_canvas.InvertSelection(); SetStatus(L"SELECTION INVERTED"); }
void WorkspaceRuntime::OnQuit() { m_wantsClose = true; }
void WorkspaceRuntime::OnSelectTool(ToolType tool) { SwitchTool(tool); }
void WorkspaceRuntime::OnSetColor(const Color& color) { m_canvas.SetColor(color); SetStatus(L"COLOR"); }
void WorkspaceRuntime::OnAddLayer() { m_canvas.AddRasterLayer(); MarkProjectDirty(); SetStatus(L"LAYER ADDED"); }
void WorkspaceRuntime::OnDeleteLayer() { m_canvas.DeleteActiveLayer(); MarkProjectDirty(); SetStatus(L"LAYER DELETED"); }
void WorkspaceRuntime::OnToggleActiveLayerVisibility() { m_canvas.ToggleActiveLayerVisibility(); MarkProjectDirty(); SetStatus(L"VISIBILITY"); }
void WorkspaceRuntime::OnToggleActiveLayerLock() { m_canvas.ToggleActiveLayerLock(); MarkProjectDirty(); SetStatus(L"LOCK"); }
void WorkspaceRuntime::OnToggleActiveLayerProtectAlpha() { m_canvas.ToggleActiveLayerProtectAlpha(); MarkProjectDirty(); SetStatus(L"PROTECT ALPHA"); }
void WorkspaceRuntime::OnToggleActiveLayerSolo() { m_canvas.ToggleActiveLayerSolo(); MarkProjectDirty(); SetStatus(L"SOLO"); }
void WorkspaceRuntime::OnDuplicateLayer() { m_canvas.DuplicateActiveLayer(); MarkProjectDirty(); SetStatus(L"DUPLICATED"); }
void WorkspaceRuntime::OnMergeLayerDown() { m_canvas.MergeActiveLayerDown(); MarkProjectDirty(); SetStatus(L"MERGED"); }
void WorkspaceRuntime::OnSetLayerOpacity(uint8_t opacity) { m_canvas.SetActiveLayerOpacity(opacity); MarkProjectDirty(); SetStatus(L"OPACITY"); }
void WorkspaceRuntime::OnToggleLayerBlendDropdown() {
    m_uiState.layerBlendDropdownOpen = !m_uiState.layerBlendDropdownOpen;
    TouchWorkspace();
}
void WorkspaceRuntime::OnSetBlendMode(uint32_t mode) {
    m_canvas.SetActiveLayerBlendMode(static_cast<BlendMode>(mode));
    m_uiState.layerBlendDropdownOpen = false;
    MarkProjectDirty();
    SetStatus(BlendName(static_cast<BlendMode>(mode)));
}
void WorkspaceRuntime::OnSelectLayer(size_t index) { m_canvas.SelectLayer(index); SetStatus(L"LAYER SELECTED"); }
void WorkspaceRuntime::OnSetBrushParam(BrushParamId param, uint16_t value) { m_canvas.SetBrushParam(param, value); SetStatus(L"BRUSH PARAM"); }
void WorkspaceRuntime::OnSetBrushTip(uint32_t tip) { m_canvas.SetBrushTip(static_cast<Render::BrushTipType>(tip)); SetStatus(TipName(static_cast<Render::BrushTipType>(tip))); }
void WorkspaceRuntime::OnSetBrushPreset(uint16_t size, uint16_t tip) {
    m_canvas.ApplyBrushPreset(size, tip);
    SetStatus(L"BRUSH PRESET");
}
void WorkspaceRuntime::OnApplyFilter(FilterCommandId filter, uint16_t value) { m_canvas.ApplyFilter(filter, value); MarkProjectDirty(); m_modals.Close(); SetStatus(L"FILTER"); }
void WorkspaceRuntime::OnCreateTextLayer(const String& text) {
    String content = text;
    float size = 36.0f;
    const size_t sep = content.find(L'|');
    if (sep != String::npos) {
        const String sizeText = content.substr(sep + 1);
        content = content.substr(0, sep);
        try {
            size = std::stof(sizeText);
        } catch (...) {
            size = 36.0f;
        }
    }
    m_canvas.AddTextLayer(content, size);
    MarkProjectDirty();
    m_modals.Close();
    SetStatus(L"TEXT LAYER");
}
void WorkspaceRuntime::OnSetZoomPreset(uint16_t zoomPercent) { m_canvas.SetZoom(std::max<uint16_t>(5, zoomPercent) / 100.0f); SetStatus(L"ZOOM"); }
void WorkspaceRuntime::OnZoomIn() { m_canvas.ZoomBy(1.25f); SetStatus(L"ZOOM IN"); }
void WorkspaceRuntime::OnZoomOut() { m_canvas.ZoomBy(0.8f); SetStatus(L"ZOOM OUT"); }
void WorkspaceRuntime::OnFitCanvas() { m_canvas.FitCanvas(); SetStatus(L"FIT"); }
void WorkspaceRuntime::OnResetView() { m_canvas.ResetView(); SetStatus(L"RESET VIEW"); }
void WorkspaceRuntime::OnToggleViewOption(ViewOptionId option) { m_canvas.ToggleViewOption(option); SetStatus(L"VIEW OPTION"); }
void WorkspaceRuntime::OnSetSnapMode(SnapModeId mode) { m_canvas.SetSnapMode(mode); SetStatus(SnapModeName(mode)); }
void WorkspaceRuntime::OnTogglePanel(WorkspacePanelId panel) {
    m_uiState.TogglePanel(panel);
    if (panel == WorkspacePanelId::Layer && !m_uiState.IsPanelVisible(WorkspacePanelId::Layer)) {
        m_uiState.layerBlendDropdownOpen = false;
    }
    RecomputeLayout();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    SetStatus(L"PANEL TOGGLED");
}
void WorkspaceRuntime::OnToggleLeftSidebar() {
    m_uiState.leftSidebarExpanded = !m_uiState.leftSidebarExpanded;
    RecomputeLayout();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    TouchWorkspace();
    SetStatus(m_uiState.leftSidebarExpanded ? L"LEFT PANEL OPEN" : L"LEFT PANEL CLOSED");
}
void WorkspaceRuntime::OnToggleRightSidebar() {
    m_uiState.rightSidebarExpanded = !m_uiState.rightSidebarExpanded;
    RecomputeLayout();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    TouchWorkspace();
    SetStatus(m_uiState.rightSidebarExpanded ? L"RIGHT PANEL OPEN" : L"RIGHT PANEL CLOSED");
}
void WorkspaceRuntime::OnInitializeLayout() {
    m_uiState.Reset(m_viewport);
    RecomputeLayout();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    SetStatus(L"LAYOUT INITIALIZED");
}
void WorkspaceRuntime::OnShowUnavailable() { SetStatus(L"NOT AVAILABLE IN MVP"); }
void WorkspaceRuntime::OnCloseWorkspace() {
    auto context = BuildActionContext();
    WorkspaceActionController::CloseWorkspaceProject(context, BuildActionCallbacks());
}

} // namespace CloverPic::Core
