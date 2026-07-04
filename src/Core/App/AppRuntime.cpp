#include "Core/App/AppRuntime.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/App/AppSceneBuilder.h"
#include "Core/Services/CoreServices.h"
#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cwctype>
#include <sstream>

namespace CloverPic::Core {

namespace {

constexpr int TopBarH = 34;
constexpr int StatusBarH = 28;
constexpr int ToolBarW = 48;
constexpr int LeftPanelW = 236;
constexpr int RightPanelW = 252;
constexpr uint32_t KeyBracketLeft = 0xDB;
constexpr uint32_t KeyBracketRight = 0xDD;
constexpr Presentation::UiNodeId CanvasAnimationRegionId = 1;

String ToolName(ToolType tool) {
    switch (tool) {
        case ToolType::Brush: return L"BRUSH";
        case ToolType::Eraser: return L"ERASER";
        case ToolType::Move: return L"MOVE";
        case ToolType::Fill: return L"FILL";
        case ToolType::Eyedropper: return L"PICK";
        default: return L"TOOL";
    }
}

bool IsHomeTextKey(uint32_t keyCode) {
    return (keyCode >= 'A' && keyCode <= 'Z') ||
           (keyCode >= '0' && keyCode <= '9') ||
           keyCode == 0x20 ||
           keyCode == 0xBD ||
           keyCode == 0xBE;
}

wchar_t CharFromKey(uint32_t keyCode, bool shift) {
    if (keyCode >= 'A' && keyCode <= 'Z') {
        const wchar_t ch = static_cast<wchar_t>(keyCode);
        return shift ? ch : static_cast<wchar_t>(std::towlower(ch));
    }
    if (keyCode >= '0' && keyCode <= '9') return static_cast<wchar_t>(keyCode);
    if (keyCode == 0x20) return L' ';
    if (keyCode == 0xBD) return L'-';
    if (keyCode == 0xBE) return L'.';
    return 0;
}

} // namespace

bool AppRuntime::Initialize() {
    if (auto* fonts = CoreServices::GetFontCatalogProvider()) {
        CoreText::CoreTextEngine::Get().Initialize(fonts->LoadFontCatalog());
    }
    m_status = L"CLOVERPIC CORE UI";
    BuildScene();
    return true;
}

void AppRuntime::Resize(uint32_t width, uint32_t height, float dpiScale) {
    m_viewport = Size(static_cast<int32_t>(width), static_cast<int32_t>(height));
    m_dpiScale = dpiScale;
    m_frame.Resize(width, height);
    if (m_screen == AppScreen::Workspace) {
        m_canvas.ResizeViewport(CanvasRect());
    }
    BuildScene();
    m_scheduler.Reset();
    MarkDirty(FullRect());
}

const RgbaFrame& AppRuntime::Render(uint64_t nowMs, std::vector<Rect>& outDirtyRects) {
    (void)nowMs;
    if (m_frame.IsEmpty()) {
        outDirtyRects.clear();
        return m_frame;
    }

    Presentation::SoftRenderer renderer(m_frame);
    if (m_screen == AppScreen::Home) {
        RenderHome(renderer);
    } else {
        RenderWorkspace(renderer);
    }
    if (m_modals.HasModal()) {
        RenderModal(renderer);
    }

    outDirtyRects = m_scheduler.ConsumeDirtyRects(nowMs, FullRect());
    if (outDirtyRects.empty()) {
        outDirtyRects.push_back(FullRect());
    }
    return m_frame;
}

void AppRuntime::HandlePointer(const Input::PointerEvent& event) {
    if (m_frame.IsEmpty()) return;

    if (!m_modals.HasModal() && m_screen == AppScreen::Workspace && CanvasRect().Contains(event.position)) {
        m_canvas.HandlePointer(event, CanvasRect());
        if (m_canvas.IsInteracting()) {
            m_scheduler.SetAnimatedRegion(CanvasAnimationRegionId, CanvasRect(), 60);
        } else {
            m_scheduler.ClearAnimatedRegion(CanvasAnimationRegionId);
        }
        MarkDirty(CanvasRect());
        return;
    }

    auto* hit = m_scene.HitTest(event.position);
    const Presentation::UiNodeId hitId = hit ? hit->id : 0;
    if (event.action == Input::PointerAction::Move && hitId != m_scene.GetHover()) {
        m_scene.SetHover(hitId);
        MarkDirty(FullRect());
    }
    if (event.action == Input::PointerAction::Down) {
        m_scene.SetCapture(hitId);
        m_scene.SetFocus(hitId);
        MarkDirty(FullRect());
    } else if (event.action == Input::PointerAction::Up) {
        if (hit && hit->id == m_scene.GetCapture()) {
            DispatchCommand(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
        }
        m_scene.SetCapture(0);
        MarkDirty(FullRect());
    }
}

void AppRuntime::HandleKey(const Input::KeyEvent& event) {
    if (event.action != Input::KeyAction::Down) return;
    if (m_screen == AppScreen::Home && HandleHomeSearchKey(event)) {
        return;
    }

    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;

    if (event.keyCode == 0x1B) {
        if (m_modals.HasModal()) {
            OnCloseModal();
        } else {
            m_wantsQuit = true;
        }
    } else if (event.keyCode == 'B') {
        SwitchTool(ToolType::Brush);
    } else if (event.keyCode == 'E') {
        SwitchTool(ToolType::Eraser);
    } else if (event.keyCode == 'N' && ctrl) {
        NewDefaultCanvas();
    } else if (event.keyCode == 'O' && ctrl) {
        OpenProject();
    } else if (event.keyCode == 'S' && ctrl) {
        SaveProject(shift);
    } else if (event.keyCode == 'Z' && ctrl) {
        m_canvas.GetHistoryManager()->Undo();
        MarkDirty(CanvasRect());
    } else if (event.keyCode == 'Y' && ctrl) {
        m_canvas.GetHistoryManager()->Redo();
        MarkDirty(CanvasRect());
    } else if (event.keyCode == KeyBracketLeft) {
        m_canvas.SetBrushSize(std::max(1.0f, m_canvas.GetBrushSize() - 2.0f));
        MarkDirty(FullRect());
    } else if (event.keyCode == KeyBracketRight) {
        m_canvas.SetBrushSize(std::min(5000.0f, m_canvas.GetBrushSize() + 2.0f));
        MarkDirty(FullRect());
    }
}

void AppRuntime::HandleWheel(int delta, const Point& position) {
    if (!m_modals.HasModal() && m_screen == AppScreen::Workspace && CanvasRect().Contains(position)) {
        m_canvas.HandleWheel(delta, position, CanvasRect());
        MarkDirty(CanvasRect());
    }
}

bool AppRuntime::NeedsFrame(uint64_t nowMs) const {
    return m_scheduler.HasPendingFrame(nowMs);
}

void AppRuntime::MarkDirty(const Rect& rect) {
    m_scheduler.Invalidate(rect);
}

void AppRuntime::BuildScene() {
    AppSceneBuilder::Build(m_screen, m_modals.Current(), m_viewport, m_frame, m_session, m_canvas, m_homeSearchQuery, m_scene);
}

void AppRuntime::RenderHome(Presentation::SoftRenderer& renderer) {
    renderer.Clear(Color(17, 17, 17));
    renderer.FillRect(Rect(0, 0, m_viewport.width, 6), Color(0, 120, 215));
    renderer.FillRect(Rect(0, 6, std::max(280, m_viewport.width / 3), m_viewport.height), Color(24, 24, 24));
    renderer.FillRect(Rect(std::max(280, m_viewport.width / 3), 6, m_viewport.width, m_viewport.height), Color(18, 18, 18));

    const int margin = std::max(24, m_viewport.width / 36);
    const int leftW = std::clamp(m_viewport.width / 4, 260, 360);
    const int tileLeft = margin + leftW + std::max(32, m_viewport.width / 24);
    renderer.FillCircle(margin + 18, margin + 36, 18, Color(245, 245, 245));
    renderer.DrawText(margin + 48, margin + 27, L"CLOVERPIC", Color(242, 242, 242), 2);
    renderer.DrawText(margin, margin + 82, L"RECENT PROJECTS", Color(150, 150, 150), 2);
    renderer.DrawText(tileLeft, margin + 34, L"START", Color(235, 235, 235), 2);
    renderer.DrawText(tileLeft + 260, margin + 34, L"CLOVER CREATION", Color(155, 155, 155), 2);

    for (const auto& node : m_scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) {
            RenderHomeAction(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::RecentItem) {
            RenderHomeRecent(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::SearchBox) {
            RenderHomeSearch(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::Tile) {
            RenderHomeTile(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::Text) {
            renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(118, 118, 118), 2);
        } else if (node.type == CoreUI::UiNodeType::Button) {
            RenderButton(renderer, node);
        }
    }
}

void AppRuntime::RenderWorkspace(Presentation::SoftRenderer& renderer) {
    renderer.Clear(Color(39, 42, 47));
    renderer.FillRect(Rect(0, 0, m_viewport.width, TopBarH), Color(25, 28, 32));
    renderer.FillRect(Rect(0, TopBarH, ToolBarW, m_viewport.height - StatusBarH), Color(29, 32, 36));
    renderer.FillRect(Rect(ToolBarW, TopBarH, ToolBarW + LeftPanelW, m_viewport.height - StatusBarH), Color(45, 49, 55));
    renderer.FillRect(Rect(m_viewport.width - RightPanelW, TopBarH, m_viewport.width, m_viewport.height - StatusBarH), Color(45, 49, 55));
    renderer.FillRect(Rect(0, m_viewport.height - StatusBarH, m_viewport.width, m_viewport.height), Color(25, 28, 32));

    RenderPanel(renderer, Rect(ToolBarW + 10, TopBarH + 12, ToolBarW + LeftPanelW - 10, TopBarH + 122), L"COLOR");
    RenderPanel(renderer, Rect(ToolBarW + 10, TopBarH + 134, ToolBarW + LeftPanelW - 10, TopBarH + 246), L"BRUSH");
    RenderPanel(renderer, Rect(m_viewport.width - RightPanelW + 10, TopBarH + 12, m_viewport.width - 10, TopBarH + 178), L"NAVIGATOR");
    RenderPanel(renderer, Rect(m_viewport.width - RightPanelW + 10, TopBarH + 190, m_viewport.width - 10, m_viewport.height - StatusBarH - 12), L"LAYERS");

    for (const auto& node : m_scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::Button) {
            RenderButton(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::ToolbarItem) {
            RenderButton(renderer, node, node.userData == static_cast<uint64_t>(m_canvas.GetTool()));
        } else if (node.type == CoreUI::UiNodeType::Swatch) {
            RenderSwatch(renderer, node);
        } else if (node.type == CoreUI::UiNodeType::LayerItem) {
            RenderLayerItem(renderer, node);
        }
    }

    m_canvas.Render(renderer, CanvasRect());

    std::wstringstream status;
    status << m_status << L" | " << ToolName(m_canvas.GetTool()) << L" | SIZE " << static_cast<int>(m_canvas.GetBrushSize())
           << L" | ZOOM " << static_cast<int>(m_canvas.GetZoom() * 100) << L"%";
    renderer.DrawText(10, m_viewport.height - 20, status.str(), Color(210, 216, 220), 2);
}

void AppRuntime::RenderModal(Presentation::SoftRenderer& renderer) {
    renderer.FillRect(FullRect(), Color(0, 0, 0, 112));

    const int modalW = 420;
    const int modalH = 286;
    const int left = std::max(20, m_viewport.width / 2 - modalW / 2);
    const int top = std::max(20, m_viewport.height / 2 - modalH / 2);
    const Rect modalRect(left, top, left + modalW, top + modalH);
    renderer.FillRect(modalRect, Color(40, 45, 51));
    renderer.StrokeRect(modalRect, Color(120, 132, 142), 1);
    renderer.DrawText(left + 30, top + 28, L"NEW CANVAS", Color(242, 246, 248), 3);
    renderer.DrawText(left + 32, top + 58, L"CHOOSE A CORE-SCENE PRESET", Color(150, 164, 174), 2);

    for (const auto& node : m_scene.Nodes()) {
        if (node.zOrder >= 100 && node.type == CoreUI::UiNodeType::Button) {
            RenderButton(renderer, node);
        }
    }
}

void AppRuntime::RenderButton(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, bool active) {
    const bool hovered = node.id == m_scene.GetHover();
    Color fill = active ? Color(73, 116, 154) : (hovered ? Color(76, 84, 94) : Color(56, 62, 70));
    renderer.FillRect(node.bounds, fill);
    renderer.StrokeRect(node.bounds, Color(112, 122, 132), 1);
    renderer.DrawText(node.bounds.left + 8, node.bounds.top + 9, node.label, Color(242, 245, 247), 2);
}

void AppRuntime::RenderHomeAction(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    renderer.FillRect(node.bounds, hovered ? Color(48, 48, 48) : Color(24, 24, 24));
    const Color accent = node.command == static_cast<uint32_t>(AppCommand::Quit) ? Color(210, 52, 56) : Color(0, 120, 215);
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 5, node.bounds.bottom), accent);
    renderer.DrawText(node.bounds.left + 16, node.bounds.top + 13, node.label, Color(238, 238, 238), 2);
}

void AppRuntime::RenderHomeRecent(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    renderer.FillRect(node.bounds, hovered ? Color(52, 52, 52) : Color(24, 24, 24));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top + 6, node.bounds.left + 30, node.bounds.top + 36), Color(0, 120, 215));
    renderer.DrawText(node.bounds.left + 42, node.bounds.top + 7, node.label, Color(238, 238, 238), 2);
    renderer.DrawText(node.bounds.right - 18, node.bounds.top + 9, L">", Color(120, 120, 120), 2);
}

void AppRuntime::RenderHomeSearch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool focused = node.id == m_scene.GetFocus();
    renderer.FillRect(node.bounds, Color(239, 239, 239));
    renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : Color(239, 239, 239), focused ? 3 : 1);
    const bool showingPlaceholder = m_homeSearchQuery.empty();
    const String text = showingPlaceholder ? L"Search recent projects" : m_homeSearchQuery;
    renderer.DrawText(node.bounds.left + 14, node.bounds.top + 14, text, showingPlaceholder ? Color(112, 112, 112) : Color(32, 32, 32), 2);
    if (focused) {
        const int caretX = node.bounds.left + 14 + static_cast<int>(m_homeSearchQuery.size()) * 12;
        renderer.FillRect(Rect(caretX, node.bounds.top + 11, caretX + 2, node.bounds.bottom - 11), Color(0, 120, 215));
    }
}

void AppRuntime::RenderHomeTile(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    Color fill = ColorFromPackedRgba(node.userData);
    if (hovered) {
        fill = Color::Interpolate(fill, Color(255, 255, 255), 0.12f);
    }
    renderer.FillRect(node.bounds, fill);
    renderer.StrokeRect(node.bounds, Color(18, 18, 18), 2);
    if (!node.label.empty()) {
        renderer.DrawText(node.bounds.left + 10, node.bounds.bottom - 24, node.label, Color(245, 248, 250), 2);
    }
}

void AppRuntime::RenderLayerItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    auto* lm = m_canvas.GetLayerManager();
    const bool active = lm && node.userData == lm->GetActiveLayerIndex();
    const bool hovered = node.id == m_scene.GetHover();
    Color fill = active ? Color(70, 91, 115) : (hovered ? Color(68, 74, 82) : Color(57, 62, 69));
    renderer.FillRect(node.bounds, fill);
    renderer.StrokeRect(node.bounds, active ? Color(120, 162, 202) : Color(76, 83, 91), 1);
    renderer.DrawText(node.bounds.left + 8, node.bounds.top + 8, node.label, Color(232, 236, 239), 2);
}

void AppRuntime::RenderSwatch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const Color swatch = ColorFromPackedRgba(node.userData);
    const bool active = swatch.r == m_canvas.GetColor().r && swatch.g == m_canvas.GetColor().g && swatch.b == m_canvas.GetColor().b;
    renderer.FillRect(node.bounds, swatch);
    renderer.StrokeRect(node.bounds, active ? Color(245, 250, 255) : Color(112, 122, 132), active ? 2 : 1);
}

void AppRuntime::RenderPanel(Presentation::SoftRenderer& renderer, const Rect& rect, const String& title) {
    renderer.FillRect(rect, Color(51, 56, 63));
    renderer.FillRect(Rect(rect.left, rect.top, rect.right, rect.top + 26), Color(35, 39, 44));
    renderer.StrokeRect(rect, Color(72, 78, 86), 1);
    renderer.DrawText(rect.left + 10, rect.top + 8, title, Color(220, 225, 228), 2);
}

void AppRuntime::NewDefaultCanvas() {
    NewCanvas(1600, 1000, 350.0f, true);
}

void AppRuntime::NewCanvas(uint32_t width, uint32_t height, float dpi, bool transparent) {
    m_canvas.NewCanvas(width, height, dpi, transparent);
    m_session.AttachProject(m_canvas.GetProject());
    m_screen = AppScreen::Workspace;
    m_modals.Close();
    BuildScene();
    m_canvas.ResizeViewport(CanvasRect());
    m_status = L"NEW CANVAS";
    MarkDirty(FullRect());
}

void AppRuntime::OpenProject() {
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) return;
    String path;
    if (!dialogs->PickOpenProjectPath(path)) return;
    OpenProjectPath(path);
}

void AppRuntime::OpenProjectPath(const String& path) {
    if (m_session.OpenProject(path, m_canvas.GetLayerManager())) {
        m_canvas.AttachLoadedProject(m_session.GetProject());
        m_screen = AppScreen::Workspace;
        BuildScene();
        m_canvas.ResizeViewport(CanvasRect());
        m_status = L"OPENED";
        MarkDirty(FullRect());
    } else {
        m_status = L"OPEN FAILED";
        MarkDirty(FullRect());
    }
}

void AppRuntime::DispatchCommand(AppCommand command, uint64_t userData, const String& payload) {
    m_commands.Dispatch(*this, command, userData, payload);
}

void AppRuntime::SaveProject(bool saveAs) {
    if (!m_canvas.GetProject() || !m_canvas.GetLayerManager()) return;
    bool ok = false;
    if (saveAs || m_session.GetCurrentFilePath().empty()) {
        auto* dialogs = CoreServices::GetFileDialogService();
        if (!dialogs) return;
        String path = m_session.GetCurrentFilePath();
        if (!dialogs->PickSaveProjectPath(path)) return;
        ok = m_session.SaveProjectAs(path, m_canvas.GetLayerManager());
    } else {
        ok = m_session.SaveProject(m_canvas.GetLayerManager());
    }
    m_status = ok ? L"SAVED" : L"SAVE FAILED";
    MarkDirty(FullRect());
}

void AppRuntime::ExportPng() {
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs || !m_canvas.GetLayerManager()) return;
    String path;
    if (!dialogs->PickExportImagePath(path)) return;
    m_status = m_session.ExportPNG(path, m_canvas.GetLayerManager()) ? L"EXPORTED" : L"EXPORT FAILED";
    MarkDirty(FullRect());
}

void AppRuntime::SwitchTool(ToolType tool) {
    m_canvas.SetTool(tool);
    m_status = ToolName(tool);
    MarkDirty(FullRect());
}

bool AppRuntime::HandleHomeSearchKey(const Input::KeyEvent& event) {
    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;
    const auto* focused = m_scene.Find(m_scene.GetFocus());
    const bool searchFocused = focused && focused->type == CoreUI::UiNodeType::SearchBox;

    if (event.keyCode == 0x1B && !m_homeSearchQuery.empty()) {
        m_homeSearchQuery.clear();
        BuildScene();
        for (const auto& node : m_scene.Nodes()) {
            if (node.type == CoreUI::UiNodeType::SearchBox) {
                m_scene.SetFocus(node.id);
                break;
            }
        }
        MarkDirty(FullRect());
        return true;
    }

    if (!searchFocused || ctrl) {
        return false;
    }

    if (event.keyCode == 0x08) {
        if (!m_homeSearchQuery.empty()) {
            m_homeSearchQuery.pop_back();
            BuildScene();
            for (const auto& node : m_scene.Nodes()) {
                if (node.type == CoreUI::UiNodeType::SearchBox) {
                    m_scene.SetFocus(node.id);
                    break;
                }
            }
            MarkDirty(FullRect());
        }
        return true;
    }

    if (IsHomeTextKey(event.keyCode) && m_homeSearchQuery.size() < 48) {
        const wchar_t ch = CharFromKey(event.keyCode, shift);
        if (ch != 0) {
            m_homeSearchQuery.push_back(ch);
            BuildScene();
            for (const auto& node : m_scene.Nodes()) {
                if (node.type == CoreUI::UiNodeType::SearchBox) {
                    m_scene.SetFocus(node.id);
                    break;
                }
            }
            MarkDirty(FullRect());
            return true;
        }
    }

    return false;
}

void AppRuntime::AddLayer() {
    auto* lm = m_canvas.GetLayerManager();
    if (!lm) return;
    lm->AddLayer(L"Layer", LayerType::Transparent);
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::DeleteLayer() {
    auto* lm = m_canvas.GetLayerManager();
    if (!lm || lm->GetLayerCount() <= 1) return;
    lm->DeleteLayer(lm->GetActiveLayerIndex());
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::ToggleActiveLayerVisibility() {
    auto* lm = m_canvas.GetLayerManager();
    if (!lm) return;
    lm->ToggleLayerVisibility(lm->GetActiveLayerIndex());
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::OnGoHome() {
    m_screen = AppScreen::Home;
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::OnShowNewCanvasModal() {
    m_modals.Show(ModalKind::NewCanvas);
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::OnCreateCanvasPreset(uint32_t width, uint32_t height) {
    NewCanvas(width, height, 350.0f, true);
}

void AppRuntime::OnCloseModal() {
    m_modals.Close();
    BuildScene();
    MarkDirty(FullRect());
}

void AppRuntime::OnOpenProject() {
    OpenProject();
}

void AppRuntime::OnOpenRecentProject(const String& path) {
    OpenProjectPath(path);
}

void AppRuntime::OnFocusHomeSearch() {
    MarkDirty(FullRect());
}

void AppRuntime::OnSaveProject(bool saveAs) {
    SaveProject(saveAs);
}

void AppRuntime::OnExportPng() {
    ExportPng();
}

void AppRuntime::OnUndo() {
    m_canvas.GetHistoryManager()->Undo();
    MarkDirty(CanvasRect());
}

void AppRuntime::OnRedo() {
    m_canvas.GetHistoryManager()->Redo();
    MarkDirty(CanvasRect());
}

void AppRuntime::OnQuit() {
    m_wantsQuit = true;
}

void AppRuntime::OnSelectTool(ToolType tool) {
    SwitchTool(tool);
}

void AppRuntime::OnSetColor(const Color& color) {
    m_canvas.SetColor(color);
    MarkDirty(FullRect());
}

void AppRuntime::OnAddLayer() {
    AddLayer();
}

void AppRuntime::OnDeleteLayer() {
    DeleteLayer();
}

void AppRuntime::OnToggleActiveLayerVisibility() {
    ToggleActiveLayerVisibility();
}

void AppRuntime::OnSelectLayer(size_t index) {
    m_canvas.SelectLayer(index);
    m_status = L"LAYER SELECTED";
    BuildScene();
    MarkDirty(FullRect());
}

Rect AppRuntime::CanvasRect() const {
    return Rect(ToolBarW + LeftPanelW, TopBarH, m_viewport.width - RightPanelW, m_viewport.height - StatusBarH);
}

} // namespace CloverPic::Core
