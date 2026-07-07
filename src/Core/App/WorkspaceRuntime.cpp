#include "Core/App/WorkspaceRuntime.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/App/AppSceneBuilder.h"
#include "Core/BlendMode.h"
#include "Core/Layer.h"
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

constexpr int MenuBarH = 24;
constexpr int CommandBarH = 30;
constexpr int TopBarH = MenuBarH + CommandBarH;
constexpr int StatusBarH = 24;
constexpr int ToolBarW = 64;
constexpr int LeftPanelW = 292;
constexpr int RightPanelW = 312;
constexpr int EdgeTabW = 18;
constexpr uint32_t KeyBracketLeft = 0xDB;
constexpr uint32_t KeyBracketRight = 0xDD;
constexpr Presentation::UiNodeId CanvasAnimationRegionId = 1;
constexpr const wchar_t* CanvasSizeWidthPayload = L"canvas-size-width";
constexpr const wchar_t* CanvasSizeHeightPayload = L"canvas-size-height";
constexpr const wchar_t* CanvasAnchorPayload = L"canvas-anchor";
constexpr const char* WorkspaceUiSettingsMagic = "CLVPIC_WORKSPACE_UI_V1";

struct Hsv {
    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
};

Hsv RgbToHsv(const Color& color) {
    const float r = color.r / 255.0f;
    const float g = color.g / 255.0f;
    const float b = color.b / 255.0f;
    const float maxV = std::max({ r, g, b });
    const float minV = std::min({ r, g, b });
    const float d = maxV - minV;

    Hsv hsv;
    hsv.v = maxV;
    hsv.s = maxV <= 0.0f ? 0.0f : d / maxV;
    if (d <= 0.0001f) {
        hsv.h = 0.0f;
    } else if (maxV == r) {
        hsv.h = 60.0f * std::fmod(((g - b) / d), 6.0f);
    } else if (maxV == g) {
        hsv.h = 60.0f * (((b - r) / d) + 2.0f);
    } else {
        hsv.h = 60.0f * (((r - g) / d) + 4.0f);
    }
    if (hsv.h < 0.0f) hsv.h += 360.0f;
    return hsv;
}

Color HsvToRgb(float hueDegrees, float saturation, float value, uint8_t alpha = 255) {
    float h = std::fmod(hueDegrees, 360.0f);
    if (h < 0.0f) h += 360.0f;
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);

    const float c = value * saturation;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = value - c;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (h < 60.0f) {
        r = c; g = x;
    } else if (h < 120.0f) {
        r = x; g = c;
    } else if (h < 180.0f) {
        g = c; b = x;
    } else if (h < 240.0f) {
        g = x; b = c;
    } else if (h < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }
    return Color(static_cast<uint8_t>(std::clamp((r + m) * 255.0f, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp((g + m) * 255.0f, 0.0f, 255.0f)),
                 static_cast<uint8_t>(std::clamp((b + m) * 255.0f, 0.0f, 255.0f)),
                 alpha);
}

String LeftOfSeparator(const String& value) {
    const size_t pos = value.find(L'|');
    return pos == String::npos ? value : value.substr(0, pos);
}

String RightOfSeparator(const String& value) {
    const size_t pos = value.find(L'|');
    return pos == String::npos ? L"" : value.substr(pos + 1);
}

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

String CanvasAnchorName(uint32_t anchorIndex) {
    switch (anchorIndex) {
        case 0: return L"Top Left";
        case 1: return L"Top Center";
        case 2: return L"Top Right";
        case 3: return L"Middle Left";
        case 4: return L"Center";
        case 5: return L"Middle Right";
        case 6: return L"Bottom Left";
        case 7: return L"Bottom Center";
        case 8: return L"Bottom Right";
        default: return L"Center";
    }
}

String JoinResizeDirections(const std::vector<String>& values) {
    if (values.empty()) {
        return L"None";
    }
    String result = values.front();
    for (size_t i = 1; i < values.size(); ++i) {
        result += L", ";
        result += values[i];
    }
    return result;
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

WorkspaceRuntime::WorkspaceRuntime(WorkspaceLaunchRequest launchRequest) : m_launchRequest(std::move(launchRequest)) {}

bool WorkspaceRuntime::Initialize() {
    if (auto* fonts = CoreServices::GetFontCatalogProvider()) {
        CoreText::CoreTextEngine::Get().Initialize(fonts->LoadFontCatalog());
    }

    if (m_launchRequest.kind != WorkspaceLaunchKind::None) {
        OpenLaunchRequest(m_launchRequest);
    } else {
        m_status = L"NO PROJECT";
    }
    LoadWorkspaceUiSettings();

    BuildScene();
    MarkDirty(FullRect());
    return true;
}

bool WorkspaceRuntime::OpenLaunchRequest(const WorkspaceLaunchRequest& launchRequest) {
    if (launchRequest.kind == WorkspaceLaunchKind::OpenProject) {
        if (m_session.OpenProject(launchRequest.projectPath, m_canvas.GetLayerManager())) {
            m_canvas.AttachLoadedProject(m_session.GetProject());
            m_canvas.ResizeViewport(CanvasRect());
            m_status = L"OPENED";
            return true;
        }
        m_status = L"OPEN FAILED";
        return false;
    }

    if (launchRequest.kind == WorkspaceLaunchKind::NewCanvas) {
        m_canvas.NewCanvas(launchRequest.width, launchRequest.height, launchRequest.dpi, launchRequest.transparent,
                           launchRequest.initialLayerType, launchRequest.backgroundColor,
                           launchRequest.rgbProfilePath, launchRequest.cmykProfilePath,
                           launchRequest.rgbProfileBytes, launchRequest.cmykProfileBytes);
        m_session.AttachProject(m_canvas.GetProject());
        m_session.MarkDirty();
        m_canvas.ResizeViewport(CanvasRect());
        m_status = L"NEW CANVAS";
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
    if (m_frame.IsEmpty()) return;

    if (event.action == Input::PointerAction::Move && m_scene.GetCapture() != 0) {
        auto* captured = m_scene.Find(m_scene.GetCapture());
        if (captured && captured->type == CoreUI::UiNodeType::Slider) {
            ApplySliderAtPoint(*captured, event.position);
            MarkDirty(FullRect());
            return;
        }
        if (captured && captured->type == CoreUI::UiNodeType::ColorField) {
            ApplyColorFieldAtPoint(*captured, event.position);
            MarkDirty(FullRect());
            return;
        }
        if (captured && captured->type == CoreUI::UiNodeType::HueStrip) {
            ApplyHueStripAtPoint(*captured, event.position);
            MarkDirty(FullRect());
            return;
        }
    }

    if (!m_modals.HasModal() && m_openMenu.empty() && CanvasRect().Contains(event.position)) {
        const bool changed = m_canvas.HandlePointer(event, CanvasRect());
        if (changed) {
            MarkProjectDirty();
        }
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
        if (hit && hit->type == CoreUI::UiNodeType::SearchBox) {
            if (hit->payload == CanvasSizeWidthPayload) m_canvasSizeField = CanvasSizeField::Width;
            else if (hit->payload == CanvasSizeHeightPayload) m_canvasSizeField = CanvasSizeField::Height;
        }
        MarkDirty(FullRect());
    } else if (event.action == Input::PointerAction::Up) {
        if (hit && hit->id == m_scene.GetCapture()) {
            if (hit->type == CoreUI::UiNodeType::MenuHeader) {
                m_openMenu = (m_openMenu == hit->payload) ? L"" : hit->payload;
                TouchWorkspace();
            } else if (hit->type == CoreUI::UiNodeType::MenuItem) {
                DispatchCommand(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
                m_openMenu.clear();
                TouchWorkspace();
            } else if (hit->type == CoreUI::UiNodeType::Slider) {
                ApplySliderAtPoint(*hit, event.position);
            } else if (hit->type == CoreUI::UiNodeType::ColorField) {
                ApplyColorFieldAtPoint(*hit, event.position);
            } else if (hit->type == CoreUI::UiNodeType::HueStrip) {
                ApplyHueStripAtPoint(*hit, event.position);
            } else {
                DispatchCommand(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
            }
        } else if (!m_openMenu.empty()) {
            m_openMenu.clear();
            TouchWorkspace();
        }
        m_scene.SetCapture(0);
        MarkDirty(FullRect());
    }
}

void WorkspaceRuntime::HandleKey(const Input::KeyEvent& event) {
    if (event.action != Input::KeyAction::Down) return;
    if (m_modals.Current() == ModalKind::CanvasSize && HandleCanvasSizeKey(event)) {
        return;
    }
    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;
    if (event.keyCode == 0x1B) {
        if (!m_openMenu.empty()) {
            m_openMenu.clear();
            TouchWorkspace();
        } else if (m_modals.HasModal()) OnCloseModal();
        else m_wantsClose = true;
    } else if (event.keyCode == 'S' && ctrl) {
        SaveProject(shift);
    } else if (event.keyCode == 'Z' && ctrl) {
        OnUndo();
    } else if (event.keyCode == 'Y' && ctrl) {
        OnRedo();
    } else if (event.keyCode == 'A' && ctrl) {
        OnSelectAll();
    } else if (event.keyCode == 'D' && ctrl) {
        OnClearSelection();
    } else if (event.keyCode == 'I' && ctrl && shift) {
        OnInvertSelection();
    } else if (event.keyCode == '0' && ctrl) {
        OnFitCanvas();
    } else if (event.keyCode == '1' && ctrl) {
        OnSetZoomPreset(100);
    } else if (event.keyCode == 'K' && ctrl) {
        OnShowSettingsModal();
    } else if ((event.keyCode == 0xBB || event.keyCode == 0x6B) && ctrl) {
        OnZoomIn();
    } else if ((event.keyCode == 0xBD || event.keyCode == 0x6D) && ctrl) {
        OnZoomOut();
    } else if (event.keyCode == 'B') {
        SwitchTool(ToolType::Brush);
    } else if (event.keyCode == 'E') {
        SwitchTool(ToolType::Eraser);
    } else if (event.keyCode == 'V') {
        SwitchTool(ToolType::Move);
    } else if (event.keyCode == 'I') {
        SwitchTool(ToolType::Eyedropper);
    } else if (event.keyCode == 'G') {
        SwitchTool(ToolType::Fill);
    } else if (event.keyCode == 'T') {
        SetStatus(L"TEXT TOOL NEXT");
    } else if (event.keyCode == KeyBracketLeft) {
        m_canvas.SetBrushSize(std::max(1.0f, m_canvas.GetBrushSize() - 2.0f));
        TouchWorkspace();
    } else if (event.keyCode == KeyBracketRight) {
        m_canvas.SetBrushSize(std::min(5000.0f, m_canvas.GetBrushSize() + 2.0f));
        TouchWorkspace();
    }
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
    const ModalKind sceneModal = m_modals.Current() == ModalKind::CanvasSize ? ModalKind::None : m_modals.Current();
    AppSceneBuilder::Build(AppScreen::Workspace, sceneModal, m_viewport, m_frame, m_session, m_canvas, L"", m_scene, m_openMenu);
    if (m_modals.Current() == ModalKind::CanvasSize) {
        BuildCanvasSizeModalNodes();
    }
}

void WorkspaceRuntime::LoadWorkspaceUiSettings() {
    auto* store = CoreServices::GetAppSettingsStore();
    if (!store) {
        return;
    }
    std::vector<uint8_t> bytes;
    if (!store->LoadSettingsBytes(bytes) || bytes.empty()) {
        return;
    }
    const std::string magic = WorkspaceUiSettingsMagic;
    auto it = std::search(bytes.begin(), bytes.end(), magic.begin(), magic.end());
    if (it == bytes.end()) {
        return;
    }
    const size_t payloadOffset = static_cast<size_t>(std::distance(bytes.begin(), it)) + magic.size();
    const std::string payload(payloadOffset < bytes.size()
                                  ? reinterpret_cast<const char*>(bytes.data() + payloadOffset)
                                  : "",
                              bytes.size() - std::min(payloadOffset, bytes.size()));
    auto contains = [&](const std::string& key, const std::string& value) {
        return payload.find("\"" + key + "\":" + value) != std::string::npos;
    };
    auto loadBool = [&](const std::string& key, bool fallback) {
        if (contains(key, "false")) return false;
        if (contains(key, "true")) return true;
        return fallback;
    };
    m_canvas.SetLeftSidebarExpanded(!contains("leftSidebarExpanded", "false"));
    m_canvas.SetRightSidebarExpanded(!contains("rightSidebarExpanded", "false"));
    m_canvas.SetPanelVisible(WorkspacePanelId::Color, loadBool("panelColor", m_canvas.IsPanelVisible(WorkspacePanelId::Color)));
    m_canvas.SetPanelVisible(WorkspacePanelId::BrushPreview, loadBool("panelBrushPreview", m_canvas.IsPanelVisible(WorkspacePanelId::BrushPreview)));
    m_canvas.SetPanelVisible(WorkspacePanelId::BrushControl, loadBool("panelBrushControl", m_canvas.IsPanelVisible(WorkspacePanelId::BrushControl)));
    m_canvas.SetPanelVisible(WorkspacePanelId::BrushPresets, loadBool("panelBrushPresets", m_canvas.IsPanelVisible(WorkspacePanelId::BrushPresets)));
    m_canvas.SetPanelVisible(WorkspacePanelId::Navigator, loadBool("panelNavigator", m_canvas.IsPanelVisible(WorkspacePanelId::Navigator)));
    m_canvas.SetPanelVisible(WorkspacePanelId::Layer, loadBool("panelLayer", m_canvas.IsPanelVisible(WorkspacePanelId::Layer)));
    m_canvas.SetPanelVisible(WorkspacePanelId::BrushSize, loadBool("panelBrushSize", m_canvas.IsPanelVisible(WorkspacePanelId::BrushSize)));
    m_canvas.SetPanelVisible(WorkspacePanelId::StatusBar, loadBool("panelStatusBar", m_canvas.IsPanelVisible(WorkspacePanelId::StatusBar)));
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
    const std::string magic = WorkspaceUiSettingsMagic;
    auto it = std::search(bytes.begin(), bytes.end(), magic.begin(), magic.end());
    if (it != bytes.end()) {
        bytes.erase(it, bytes.end());
    }
    std::ostringstream json;
    json << WorkspaceUiSettingsMagic
         << "{\"workspace\":{\"leftSidebarExpanded\":"
         << (m_canvas.IsLeftSidebarExpanded() ? "true" : "false")
         << ",\"rightSidebarExpanded\":"
         << (m_canvas.IsRightSidebarExpanded() ? "true" : "false")
         << ",\"panelColor\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::Color) ? "true" : "false")
         << ",\"panelBrushPreview\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::BrushPreview) ? "true" : "false")
         << ",\"panelBrushControl\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::BrushControl) ? "true" : "false")
         << ",\"panelBrushPresets\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::BrushPresets) ? "true" : "false")
         << ",\"panelNavigator\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::Navigator) ? "true" : "false")
         << ",\"panelLayer\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::Layer) ? "true" : "false")
         << ",\"panelBrushSize\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::BrushSize) ? "true" : "false")
         << ",\"panelStatusBar\":" << (m_canvas.IsPanelVisible(WorkspacePanelId::StatusBar) ? "true" : "false")
         << "}}";
    const std::string data = json.str();
    bytes.insert(bytes.end(), data.begin(), data.end());
    store->SaveSettingsBytes(bytes);
}

void WorkspaceRuntime::BuildCanvasSizeModalNodes() {
    CoreUI::UiNode blocker;
    blocker.type = CoreUI::UiNodeType::Panel;
    blocker.bounds = FullRect();
    blocker.command = static_cast<uint32_t>(AppCommand::None);
    blocker.zOrder = 90;
    blocker.flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive;
    blocker.accessibilityLabel = L"Canvas size modal blocker";
    const auto blockerId = m_scene.AddNode(std::move(blocker));
    m_scene.PushModal(blockerId);

    const int modalW = 660;
    const int modalH = 392;
    const int left = std::max(20, m_viewport.width / 2 - modalW / 2);
    const int top = std::max(20, m_viewport.height / 2 - modalH / 2);

    auto addNode = [&](CoreUI::UiNodeType type, const Rect& rect, String label, AppCommand command = AppCommand::None,
                       uint64_t userData = 0, String payload = L"", bool interactive = true) {
        CoreUI::UiNode node;
        node.parentId = blockerId;
        node.type = type;
        node.bounds = rect;
        node.label = std::move(label);
        node.accessibilityLabel = node.label;
        node.command = static_cast<uint32_t>(command);
        node.userData = userData;
        node.payload = std::move(payload);
        node.zOrder = 100;
        node.flags = Presentation::UiNodeVisible | (interactive ? static_cast<uint32_t>(Presentation::UiNodeInteractive) : 0u);
        return m_scene.AddNode(std::move(node));
    };

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 50, left + 200, top + 72),
            L"Current canvas size", AppCommand::None, 0, L"", false);
    if (auto project = m_canvas.GetProject()) {
        std::wstringstream summary;
        summary << project->GetCanvas().widthPx << L" x " << project->GetCanvas().heightPx << L" px";
        addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 74, left + 220, top + 96),
                summary.str(), AppCommand::None, 0, L"", false);
    }

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 112, left + 150, top + 134), L"Width", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 160, left + 150, top + 182), L"Height", AppCommand::None, 0, L"", false);
    const auto widthId = addNode(CoreUI::UiNodeType::SearchBox, Rect(left + 132, top + 102, left + 252, top + 138),
                                 std::to_wstring(m_canvasResizeWidth), AppCommand::None, 0, CanvasSizeWidthPayload);
    const auto heightId = addNode(CoreUI::UiNodeType::SearchBox, Rect(left + 132, top + 150, left + 252, top + 186),
                                  std::to_wstring(m_canvasResizeHeight), AppCommand::None, 0, CanvasSizeHeightPayload);

    addNode(CoreUI::UiNodeType::Button, Rect(left + 268, top + 102, left + 354, top + 138), L"Swap",
            AppCommand::SwapCanvasOrientation);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 206, left + 150, top + 228), L"Anchor", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 230, left + 320, top + 250),
            L"Choose what stays fixed while resizing.", AppCommand::None, 0, L"", false);

    const int anchorLeft = left + 132;
    const int anchorTop = top + 236;
    for (uint32_t row = 0; row < 3; ++row) {
        for (uint32_t col = 0; col < 3; ++col) {
            const uint32_t index = row * 3 + col;
            String label = (m_canvasAnchor == static_cast<CanvasAnchor>(index)) ? L"*" : L"";
            addNode(CoreUI::UiNodeType::Button,
                    Rect(anchorLeft + static_cast<int>(col) * 32, anchorTop + static_cast<int>(row) * 32,
                         anchorLeft + static_cast<int>(col) * 32 + 28, anchorTop + static_cast<int>(row) * 32 + 28),
                    label, AppCommand::SetCanvasAnchor, index, CanvasAnchorPayload);
        }
    }

    addNode(CoreUI::UiNodeType::Text, Rect(left + 28, top + 332, left + 210, top + 350), L"Quick presets", AppCommand::None, 0, L"", false);
    addNode(CoreUI::UiNodeType::Button, Rect(left + 132, top + 350, left + 252, top + 382), L"1600 x 1000", AppCommand::CreateCanvasPreset, PackCanvasPreset(1600, 1000));
    addNode(CoreUI::UiNodeType::Button, Rect(left + 258, top + 350, left + 378, top + 382), L"1920 x 1080", AppCommand::CreateCanvasPreset, PackCanvasPreset(1920, 1080));
    addNode(CoreUI::UiNodeType::Button, Rect(left + 384, top + 350, left + 504, top + 382), L"A4 2480 x 3508", AppCommand::CreateCanvasPreset, PackCanvasPreset(2480, 3508));

    addNode(CoreUI::UiNodeType::Button, Rect(left + 524, top + 348, left + 588, top + 382), L"Apply",
            AppCommand::CreateCanvasFromForm);
    addNode(CoreUI::UiNodeType::Button, Rect(left + 594, top + 348, left + 658, top + 382), L"Cancel",
            AppCommand::CloseModal);

    if (m_canvasSizeField == CanvasSizeField::Width) {
        m_scene.SetFocus(widthId);
    } else if (m_canvasSizeField == CanvasSizeField::Height) {
        m_scene.SetFocus(heightId);
    }
}

void WorkspaceRuntime::MarkDirty(const Rect& rect) {
    m_scheduler.Invalidate(rect);
}

Rect WorkspaceRuntime::CanvasRect() const {
    const int leftDockW = m_canvas.IsLeftSidebarExpanded() ? LeftPanelW : EdgeTabW;
    const int rightDockW = m_canvas.IsRightSidebarExpanded() ? RightPanelW : EdgeTabW;
    return Rect(ToolBarW + leftDockW, TopBarH, m_viewport.width - rightDockW, m_viewport.height - StatusBarH);
}

void WorkspaceRuntime::DispatchCommand(AppCommand command, uint64_t userData, const String& payload) {
    m_commands.Dispatch(*this, command, userData, payload);
}

void WorkspaceRuntime::RenderWorkspace(Presentation::SoftRenderer& renderer) {
    renderer.Clear(Color(66, 63, 63));
    renderer.FillRect(Rect(0, 0, m_viewport.width, MenuBarH), Color(27, 29, 31));
    renderer.FillRect(Rect(0, MenuBarH, m_viewport.width, TopBarH), Color(35, 37, 40));
    renderer.StrokeRect(Rect(0, 0, m_viewport.width, TopBarH), Color(75, 80, 86), 1);
    const int leftDockW = m_canvas.IsLeftSidebarExpanded() ? LeftPanelW : EdgeTabW;
    const int rightDockW = m_canvas.IsRightSidebarExpanded() ? RightPanelW : EdgeTabW;
    renderer.FillRect(Rect(0, TopBarH, ToolBarW, m_viewport.height - StatusBarH), Color(29, 31, 34));
    renderer.FillRect(Rect(ToolBarW, TopBarH, ToolBarW + leftDockW, m_viewport.height - StatusBarH), Color(36, 38, 40));
    renderer.FillRect(Rect(m_viewport.width - rightDockW, TopBarH, m_viewport.width, m_viewport.height - StatusBarH), Color(36, 38, 40));
    if (m_canvas.IsPanelVisible(WorkspacePanelId::StatusBar)) {
        renderer.FillRect(Rect(0, m_viewport.height - StatusBarH, m_viewport.width, m_viewport.height), Color(28, 30, 32));
    }
    renderer.FillVerticalGradient(CanvasRect(), Color(82, 79, 79), Color(68, 65, 65));

    m_canvas.Render(renderer, CanvasRect());
    RenderWorkspacePanels(renderer);

    for (const auto& node : m_scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::Panel && node.zOrder >= 100) {
            renderer.FillRect(node.bounds, Color(30, 32, 34));
            renderer.StrokeRect(node.bounds, Color(68, 72, 76), 1);
        } else if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node);
        else if (node.type == CoreUI::UiNodeType::MenuHeader) RenderMenuHeader(renderer, node);
        else if (node.type == CoreUI::UiNodeType::MenuItem) RenderMenuItem(renderer, node);
        else if (node.type == CoreUI::UiNodeType::MenuSeparator) renderer.FillRect(node.bounds, Color(67, 70, 73));
        else if (node.type == CoreUI::UiNodeType::ToolbarItem) RenderButton(renderer, node, node.userData == static_cast<uint64_t>(m_canvas.GetTool()));
        else if (node.type == CoreUI::UiNodeType::Swatch) RenderSwatch(renderer, node);
        else if (node.type == CoreUI::UiNodeType::LayerItem) RenderLayerItem(renderer, node);
        else if (node.type == CoreUI::UiNodeType::Slider) RenderSlider(renderer, node);
        else if (node.type == CoreUI::UiNodeType::SearchBox) RenderSearchBox(renderer, node);
        else if (node.type == CoreUI::UiNodeType::ColorField) RenderColorField(renderer, node);
        else if (node.type == CoreUI::UiNodeType::HueStrip) RenderHueStrip(renderer, node);
        else if (node.type == CoreUI::UiNodeType::BrushPresetItem) RenderBrushPresetItem(renderer, node);
        else if (node.type == CoreUI::UiNodeType::BrushSizeChip) RenderBrushSizeChip(renderer, node);
        else if (node.type == CoreUI::UiNodeType::Text) renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(210, 216, 220), 2);
    }

    if (m_canvas.IsPanelVisible(WorkspacePanelId::StatusBar)) {
        std::wstringstream status;
        status << m_status << L" | " << ToolName(m_canvas.GetTool()) << L" | SIZE " << static_cast<int>(m_canvas.GetBrushSize())
               << L" | OP " << static_cast<int>(m_canvas.GetBrushOpacity() * 100)
               << L"% | " << TipName(m_canvas.GetBrushTip()) << L" | ZOOM " << static_cast<int>(m_canvas.GetZoom() * 100) << L"%";
        renderer.DrawText(8, m_viewport.height - 18, status.str(), Color(210, 216, 220), 2);
    }
}

void WorkspaceRuntime::RenderWorkspacePanels(Presentation::SoftRenderer& renderer) {
    if (!m_canvas.IsLeftSidebarExpanded() && !m_canvas.IsRightSidebarExpanded()) {
        return;
    }
    const int leftPanelLeft = ToolBarW + 4;
    const int leftPanelRight = ToolBarW + LeftPanelW - 4;
    const int navLeft = m_viewport.width - RightPanelW + 8;
    const int navRight = m_viewport.width - 8;
    const int colorTop = TopBarH + 4;
    const int colorFieldSize = std::min(216, leftPanelRight - (leftPanelLeft + 8) - 38);
    const int colorFieldTop = colorTop + 28;
    const int brushPreviewTop = colorFieldTop + colorFieldSize + 76;
    const int brushControlTop = brushPreviewTop + 128;
    const int presetTop = std::max(brushControlTop + 250, m_viewport.height - StatusBarH - 198);
    const int layerTop = TopBarH + 210;
    const int layerToolsY = m_viewport.height - StatusBarH - 226;
    const int sizeTop = m_viewport.height - StatusBarH - 156;

    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Color)) {
        RenderPanel(renderer, Rect(leftPanelLeft, colorTop, leftPanelRight, colorTop + colorFieldSize + 104), L"Color");
    }
    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::BrushPreview)) {
        RenderPanel(renderer, Rect(leftPanelLeft, brushPreviewTop, leftPanelRight, brushPreviewTop + 118), L"Brush Preview");
    }
    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::BrushControl)) {
        RenderPanel(renderer, Rect(leftPanelLeft, brushControlTop, leftPanelRight, presetTop - 8), L"Brush Control");
    }
    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::BrushPresets)) {
        RenderPanel(renderer, Rect(leftPanelLeft, presetTop - 28, leftPanelRight, m_viewport.height - StatusBarH - 4), L"Brush: Pen");
    }

    if (m_canvas.IsRightSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Navigator)) {
        RenderPanel(renderer, Rect(navLeft, TopBarH + 4, navRight, TopBarH + 198), L"Navigator");
    }
    if (m_canvas.IsRightSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Layer)) {
        RenderPanel(renderer, Rect(navLeft, layerTop, navRight, layerToolsY + 62), L"Layer");
    }
    if (m_canvas.IsRightSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::BrushSize)) {
        RenderPanel(renderer, Rect(navLeft, sizeTop, navRight, m_viewport.height - StatusBarH - 4), L"Brush Size");
    }

    const Color current = m_canvas.GetColor();
    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Color)) {
        renderer.FillRect(Rect(leftPanelLeft + 12, colorTop + 38, leftPanelLeft + 44, colorTop + 70), current);
        renderer.StrokeRect(Rect(leftPanelLeft + 12, colorTop + 38, leftPanelLeft + 44, colorTop + 70), Color(235, 238, 240), 2);
        renderer.FillRect(Rect(leftPanelLeft + 22, colorTop + 60, leftPanelLeft + 54, colorTop + 92), Color(255, 255, 255));
        renderer.StrokeRect(Rect(leftPanelLeft + 22, colorTop + 60, leftPanelLeft + 54, colorTop + 92), Color(30, 33, 36), 1);

        std::wstringstream rgb;
        rgb << L"R:" << static_cast<int>(current.r) << L"  G:" << static_cast<int>(current.g) << L"  B:" << static_cast<int>(current.b);
        renderer.DrawText(leftPanelLeft + 12, colorFieldTop + colorFieldSize + 58, rgb.str(), Color(202, 210, 216), 2);
    }

    if (m_canvas.IsLeftSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::BrushPreview)) {
        const Rect preview(leftPanelLeft + 10, brushPreviewTop + 28, leftPanelRight - 10, brushPreviewTop + 108);
        renderer.DrawCheckerboard(preview, 8, Color(230, 230, 230), Color(204, 204, 204));
        renderer.StrokeRect(preview, Color(78, 84, 88), 1);
        renderer.FillCircle(preview.left + 52, preview.top + 39, std::max(3, static_cast<int>(m_canvas.GetBrushSize() / 8)), Color(54, 54, 54));
        for (int i = 0; i < 8; ++i) {
            const int x0 = preview.left + 120 + i * 14;
            const int y0 = preview.top + 42 + static_cast<int>(std::sin(i * 0.7f) * 10.0f);
            const int x1 = preview.left + 120 + (i + 1) * 14;
            const int y1 = preview.top + 42 + static_cast<int>(std::sin((i + 1) * 0.7f) * 10.0f);
            renderer.DrawLine(x0, y0, x1, y1, current, std::max(2, static_cast<int>(m_canvas.GetBrushSize() / 16)));
        }
    }

    const Rect navPreview(navLeft + 16, TopBarH + 38, navRight - 16, TopBarH + 154);
    if (m_canvas.IsRightSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Navigator)) {
        renderer.DrawCheckerboard(navPreview, 10, Color(78, 80, 84), Color(65, 67, 71));
    }
    if (m_canvas.IsRightSidebarExpanded() && m_canvas.IsPanelVisible(WorkspacePanelId::Navigator)) if (auto project = m_canvas.GetProject()) {
        const auto& canvas = project->GetCanvas();
        const float aspect = canvas.heightPx == 0 ? 1.0f : static_cast<float>(canvas.widthPx) / canvas.heightPx;
        int miniW = navPreview.Width() - 28;
        int miniH = static_cast<int>(miniW / std::max(0.2f, aspect));
        if (miniH > navPreview.Height() - 20) {
            miniH = navPreview.Height() - 20;
            miniW = static_cast<int>(miniH * aspect);
        }
        const int miniL = navPreview.left + (navPreview.Width() - miniW) / 2;
        const int miniT = navPreview.top + (navPreview.Height() - miniH) / 2;
        renderer.FillRect(Rect(miniL, miniT, miniL + miniW, miniT + miniH), Color(245, 245, 245));
        renderer.StrokeRect(Rect(miniL, miniT, miniL + miniW, miniT + miniH), Color(28, 31, 34), 2);
        renderer.StrokeRect(Rect(miniL + 8, miniT + 8, miniL + miniW - 8, miniT + miniH - 8), Color(0, 120, 215), 1);
    }
}

void WorkspaceRuntime::RenderModal(Presentation::SoftRenderer& renderer) {
    renderer.FillRect(FullRect(), Color(0, 0, 0, 112));
    const int modalW = m_modals.Current() == ModalKind::CanvasSize ? 660 : 460;
    const int modalH = m_modals.Current() == ModalKind::CanvasSize ? 392 : 300;
    const int left = std::max(20, m_viewport.width / 2 - modalW / 2);
    const int top = std::max(20, m_viewport.height / 2 - modalH / 2);
    renderer.FillRect(Rect(left, top, left + modalW, top + modalH), Color(40, 45, 51));
    renderer.StrokeRect(Rect(left, top, left + modalW, top + modalH), Color(120, 132, 142), 1);
    String title = L"WORKSPACE";
    if (m_modals.Current() == ModalKind::Filters) title = L"FILTERS";
    if (m_modals.Current() == ModalKind::TextLayer) title = L"TEXT LAYER";
    if (m_modals.Current() == ModalKind::NewCanvas) title = L"NEW CANVAS";
    if (m_modals.Current() == ModalKind::CanvasSize) title = L"CANVAS SIZE";
    renderer.DrawText(left + 30, top + 28, title, Color(242, 246, 248), 3);
    if (m_modals.Current() == ModalKind::CanvasSize) {
        RenderCanvasSizePreview(renderer, Rect(left + 392, top + 82, left + 632, top + 298));
    }
    for (const auto& node : m_scene.Nodes()) {
        if (node.zOrder >= 100) {
            if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node);
            else if (node.type == CoreUI::UiNodeType::SearchBox) RenderSearchBox(renderer, node);
            else if (node.type == CoreUI::UiNodeType::Text) renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(210, 216, 220), 2);
        }
    }
}

void WorkspaceRuntime::RenderCanvasSizePreview(Presentation::SoftRenderer& renderer, const Rect& rect) {
    renderer.FillRect(rect, Color(31, 34, 38));
    renderer.FillRect(Rect(rect.left, rect.top, rect.right, rect.top + 28), Color(36, 40, 45));
    renderer.StrokeRect(rect, Color(86, 92, 98), 1);
    renderer.DrawText(rect.left + 10, rect.top + 8, L"Resize Preview", Color(232, 238, 242), 2);

    const auto project = m_canvas.GetProject();
    if (!project || m_canvasResizeWidth == 0 || m_canvasResizeHeight == 0) {
        renderer.DrawText(rect.left + 10, rect.top + 42, L"Enter a target width and height.", Color(170, 176, 182), 2);
        return;
    }

    const auto& canvas = project->GetCanvas();
    const uint32_t currentWidth = std::max<uint32_t>(1, canvas.widthPx);
    const uint32_t currentHeight = std::max<uint32_t>(1, canvas.heightPx);
    const uint32_t targetWidth = std::max<uint32_t>(1, m_canvasResizeWidth);
    const uint32_t targetHeight = std::max<uint32_t>(1, m_canvasResizeHeight);
    const uint32_t anchorIndex = static_cast<uint32_t>(m_canvasAnchor);
    const uint32_t anchorX = anchorIndex % 3;
    const uint32_t anchorY = anchorIndex / 3;

    const uint32_t srcStartX = currentWidth > targetWidth ? (anchorX * (currentWidth - targetWidth)) / 2u : 0u;
    const uint32_t srcStartY = currentHeight > targetHeight ? (anchorY * (currentHeight - targetHeight)) / 2u : 0u;
    const uint32_t dstStartX = targetWidth > currentWidth ? (anchorX * (targetWidth - currentWidth)) / 2u : 0u;
    const uint32_t dstStartY = targetHeight > currentHeight ? (anchorY * (targetHeight - currentHeight)) / 2u : 0u;
    const uint32_t cropLeftPx = currentWidth > targetWidth ? srcStartX : 0u;
    const uint32_t cropTopPx = currentHeight > targetHeight ? srcStartY : 0u;
    const uint32_t cropRightPx = currentWidth > targetWidth ? (currentWidth - targetWidth - srcStartX) : 0u;
    const uint32_t cropBottomPx = currentHeight > targetHeight ? (currentHeight - targetHeight - srcStartY) : 0u;
    const uint32_t expandLeftPx = targetWidth > currentWidth ? dstStartX : 0u;
    const uint32_t expandTopPx = targetHeight > currentHeight ? dstStartY : 0u;
    const uint32_t expandRightPx = targetWidth > currentWidth ? (targetWidth - currentWidth - dstStartX) : 0u;
    const uint32_t expandBottomPx = targetHeight > currentHeight ? (targetHeight - currentHeight - dstStartY) : 0u;

    const uint32_t spaceWidth = std::max(currentWidth, targetWidth);
    const uint32_t spaceHeight = std::max(currentHeight, targetHeight);
    const Rect previewRect(rect.left + 14, rect.top + 40, rect.right - 14, rect.bottom - 76);
    renderer.DrawCheckerboard(previewRect, 8, Color(67, 70, 74), Color(59, 62, 66));
    renderer.StrokeRect(previewRect, Color(20, 22, 24), 1);

    const float scale = std::min(
        static_cast<float>(std::max(1, previewRect.Width() - 12)) / static_cast<float>(spaceWidth),
        static_cast<float>(std::max(1, previewRect.Height() - 12)) / static_cast<float>(spaceHeight));
    const int scaledSpaceW = std::max(8, static_cast<int>(std::round(spaceWidth * scale)));
    const int scaledSpaceH = std::max(8, static_cast<int>(std::round(spaceHeight * scale)));
    const int unionLeft = previewRect.left + (previewRect.Width() - scaledSpaceW) / 2;
    const int unionTop = previewRect.top + (previewRect.Height() - scaledSpaceH) / 2;

    const int currentLeft = unionLeft + static_cast<int>(std::round((targetWidth >= currentWidth ? dstStartX : 0u) * scale));
    const int currentTop = unionTop + static_cast<int>(std::round((targetHeight >= currentHeight ? dstStartY : 0u) * scale));
    const int currentRight = currentLeft + std::max(8, static_cast<int>(std::round(currentWidth * scale)));
    const int currentBottom = currentTop + std::max(8, static_cast<int>(std::round(currentHeight * scale)));

    const int targetLeft = unionLeft + static_cast<int>(std::round((targetWidth >= currentWidth ? 0u : srcStartX) * scale));
    const int targetTop = unionTop + static_cast<int>(std::round((targetHeight >= currentHeight ? 0u : srcStartY) * scale));
    const int targetRight = targetLeft + std::max(8, static_cast<int>(std::round(targetWidth * scale)));
    const int targetBottom = targetTop + std::max(8, static_cast<int>(std::round(targetHeight * scale)));

    const Rect currentRect(currentLeft, currentTop, currentRight, currentBottom);
    const Rect targetRect(targetLeft, targetTop, targetRight, targetBottom);
    const Rect overlapRect(std::max(currentRect.left, targetRect.left),
                           std::max(currentRect.top, targetRect.top),
                           std::min(currentRect.right, targetRect.right),
                           std::min(currentRect.bottom, targetRect.bottom));

    const bool cropLeft = targetRect.left > currentRect.left;
    const bool cropTop = targetRect.top > currentRect.top;
    const bool cropRight = targetRect.right < currentRect.right;
    const bool cropBottom = targetRect.bottom < currentRect.bottom;
    const bool expandLeft = targetRect.left < currentRect.left;
    const bool expandTop = targetRect.top < currentRect.top;
    const bool expandRight = targetRect.right > currentRect.right;
    const bool expandBottom = targetRect.bottom > currentRect.bottom;
    const int widthDelta = static_cast<int>(targetWidth) - static_cast<int>(currentWidth);
    const int heightDelta = static_cast<int>(targetHeight) - static_cast<int>(currentHeight);

    auto drawArrow = [&](int x0, int y0, int x1, int y1, const Color& color) {
        renderer.DrawLine(x0, y0, x1, y1, color, 1);
        const float dx = static_cast<float>(x1 - x0);
        const float dy = static_cast<float>(y1 - y0);
        const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        const float ux = dx / length;
        const float uy = dy / length;
        const int arrowX1 = static_cast<int>(std::round(x1 - ux * 6.0f - uy * 4.0f));
        const int arrowY1 = static_cast<int>(std::round(y1 - uy * 6.0f + ux * 4.0f));
        const int arrowX2 = static_cast<int>(std::round(x1 - ux * 6.0f + uy * 4.0f));
        const int arrowY2 = static_cast<int>(std::round(y1 - uy * 6.0f - ux * 4.0f));
        renderer.DrawLine(x1, y1, arrowX1, arrowY1, color, 1);
        renderer.DrawLine(x1, y1, arrowX2, arrowY2, color, 1);
    };

    auto signedDeltaText = [](int delta) {
        std::wstringstream text;
        if (delta > 0) {
            text << L"+";
        }
        text << delta << L" px";
        return text.str();
    };

    renderer.FillRect(currentRect, Color(92, 54, 54, 210));
    renderer.FillRect(targetRect, Color(46, 78, 116, 210));
    if (overlapRect.right > overlapRect.left && overlapRect.bottom > overlapRect.top) {
        renderer.FillRect(overlapRect, Color(238, 242, 245, 255));
        renderer.StrokeRect(overlapRect, Color(24, 28, 32), 1);
    }
    renderer.StrokeRect(currentRect, Color(226, 146, 122), 1);
    renderer.StrokeRect(targetRect, Color(112, 186, 255), 2);

    if (cropLeft) {
        drawArrow(currentRect.left + 2, currentRect.top + currentRect.Height() / 2, targetRect.left - 3, currentRect.top + currentRect.Height() / 2, Color(250, 208, 188));
        renderer.DrawText(currentRect.left + 6, currentRect.top + currentRect.Height() / 2 - 18, L"-" + std::to_wstring(cropLeftPx), Color(250, 208, 188), 1);
    }
    if (cropRight) {
        drawArrow(currentRect.right - 2, currentRect.top + currentRect.Height() / 2, targetRect.right + 3, currentRect.top + currentRect.Height() / 2, Color(250, 208, 188));
        renderer.DrawText(currentRect.right - 28, currentRect.top + currentRect.Height() / 2 - 18, L"-" + std::to_wstring(cropRightPx), Color(250, 208, 188), 1);
    }
    if (cropTop) {
        drawArrow(currentRect.left + currentRect.Width() / 2, currentRect.top + 2, currentRect.left + currentRect.Width() / 2, targetRect.top - 3, Color(250, 208, 188));
        renderer.DrawText(currentRect.left + currentRect.Width() / 2 - 12, currentRect.top + 6, L"-" + std::to_wstring(cropTopPx), Color(250, 208, 188), 1);
    }
    if (cropBottom) {
        drawArrow(currentRect.left + currentRect.Width() / 2, currentRect.bottom - 2, currentRect.left + currentRect.Width() / 2, targetRect.bottom + 3, Color(250, 208, 188));
        renderer.DrawText(currentRect.left + currentRect.Width() / 2 - 12, currentRect.bottom - 18, L"-" + std::to_wstring(cropBottomPx), Color(250, 208, 188), 1);
    }
    if (expandLeft) {
        drawArrow(currentRect.left - 2, currentRect.top + currentRect.Height() / 2, targetRect.left + 3, currentRect.top + currentRect.Height() / 2, Color(220, 240, 255));
        renderer.DrawText(targetRect.left + 6, currentRect.top + currentRect.Height() / 2 - 18, L"+" + std::to_wstring(expandLeftPx), Color(220, 240, 255), 1);
    }
    if (expandRight) {
        drawArrow(currentRect.right + 2, currentRect.top + currentRect.Height() / 2, targetRect.right - 3, currentRect.top + currentRect.Height() / 2, Color(220, 240, 255));
        renderer.DrawText(targetRect.right - 28, currentRect.top + currentRect.Height() / 2 - 18, L"+" + std::to_wstring(expandRightPx), Color(220, 240, 255), 1);
    }
    if (expandTop) {
        drawArrow(currentRect.left + currentRect.Width() / 2, currentRect.top - 2, currentRect.left + currentRect.Width() / 2, targetRect.top + 3, Color(220, 240, 255));
        renderer.DrawText(targetRect.left + targetRect.Width() / 2 - 12, targetRect.top + 6, L"+" + std::to_wstring(expandTopPx), Color(220, 240, 255), 1);
    }
    if (expandBottom) {
        drawArrow(currentRect.left + currentRect.Width() / 2, currentRect.bottom + 2, currentRect.left + currentRect.Width() / 2, targetRect.bottom - 3, Color(220, 240, 255));
        renderer.DrawText(targetRect.left + targetRect.Width() / 2 - 12, targetRect.bottom - 18, L"+" + std::to_wstring(expandBottomPx), Color(220, 240, 255), 1);
    }

    std::vector<String> cropDirections;
    std::vector<String> expandDirections;
    if (cropLeft) cropDirections.push_back(L"Left");
    if (cropTop) cropDirections.push_back(L"Top");
    if (cropRight) cropDirections.push_back(L"Right");
    if (cropBottom) cropDirections.push_back(L"Bottom");
    if (expandLeft) expandDirections.push_back(L"Left");
    if (expandTop) expandDirections.push_back(L"Top");
    if (expandRight) expandDirections.push_back(L"Right");
    if (expandBottom) expandDirections.push_back(L"Bottom");

    renderer.DrawText(rect.left + 10, rect.bottom - 60, L"Anchor: " + CanvasAnchorName(anchorIndex), Color(214, 220, 224), 2);
    renderer.DrawText(rect.left + 10, rect.bottom - 42,
                      L"Delta: W " + signedDeltaText(widthDelta) + L"  H " + signedDeltaText(heightDelta),
                      Color(198, 206, 212), 1);
    renderer.DrawText(rect.left + 10, rect.bottom - 24,
                      L"Crop: " + JoinResizeDirections(cropDirections) + L"  Expand: " + JoinResizeDirections(expandDirections),
                      Color(174, 182, 188), 1);
}

void WorkspaceRuntime::RenderButton(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, bool active) {
    const bool hovered = node.id == m_scene.GetHover();
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    if (m_modals.Current() == ModalKind::CanvasSize && node.payload == CanvasAnchorPayload &&
        node.userData == static_cast<uint64_t>(m_canvasAnchor)) {
        active = true;
    }
    if (node.type == CoreUI::UiNodeType::ToolbarItem) {
        const Color railFill = disabled ? Color(29, 31, 34)
                             : active ? Color(0, 120, 215)
                             : hovered ? Color(0, 82, 150)
                                       : Color(29, 31, 34);
        const Color content = disabled ? Color(104, 110, 116) : Color(244, 248, 250);
        renderer.FillRect(node.bounds, railFill);
        renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                          active ? Color(120, 196, 255, 160) : Color(72, 78, 84, 120));
        renderer.FillRect(Rect(node.bounds.left, node.bounds.bottom - 1, node.bounds.right, node.bounds.bottom),
                          Color(12, 14, 16, 180));
        if (active) {
            renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 4, node.bounds.bottom), Color(155, 210, 255));
        } else if (hovered && !disabled) {
            renderer.StrokeRect(node.bounds, Color(70, 145, 205), 1);
        }

        const int iconSide = std::min(30, std::max(18, node.bounds.Width() - 20));
        const int iconLeft = node.bounds.left + (node.bounds.Width() - iconSide) / 2;
        const Rect iconRect(iconLeft, node.bounds.top + 7, iconLeft + iconSide, node.bounds.top + 7 + iconSide);
        if (!Presentation::IconPainter::Draw(renderer, node.iconId, iconRect, content, 2)) {
            renderer.DrawText(node.bounds.left + 8, node.bounds.top + 9, node.label, content, 1);
        }
        const int labelW = static_cast<int>(node.label.size()) * 7;
        const int labelX = node.bounds.left + std::max(2, (node.bounds.Width() - labelW) / 2);
        renderer.DrawText(labelX, node.bounds.bottom - 16, node.label, content, 1);
        return;
    }

    Color fill = disabled ? Color(42, 44, 46) : (active ? Color(0, 120, 215) : (hovered ? Color(74, 78, 82) : Color(48, 51, 54)));
    renderer.FillRect(node.bounds, fill);
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      disabled ? Color(60, 64, 68, 120) : Color(104, 110, 116, 130));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.bottom - 1, node.bounds.right, node.bounds.bottom),
                      Color(18, 20, 22, 180));
    renderer.StrokeRect(node.bounds, active ? Color(129, 190, 245) : Color(82, 88, 92), 1);
    const Color content = disabled ? Color(116, 120, 124) : Color(232, 236, 238);
    const bool hasIcon = node.iconId != Presentation::IconId::None;
    const bool centerIcon = node.iconPlacement == CoreUI::IconPlacement::Center || node.bounds.Width() <= 36;
    if (hasIcon && centerIcon) {
        const int side = std::min(node.bounds.Width(), node.bounds.Height()) - 8;
        const int left = node.bounds.left + (node.bounds.Width() - side) / 2;
        const int top = node.bounds.top + (node.bounds.Height() - side) / 2;
        if (!Presentation::IconPainter::Draw(renderer, node.iconId, Rect(left, top, left + side, top + side), content, 1)) {
            renderer.DrawText(node.bounds.left + 6, node.bounds.top + 6, node.label, content, 2);
        }
        return;
    }
    int textX = node.bounds.left + 6;
    if (hasIcon) {
        const Rect iconRect(node.bounds.left + 5, node.bounds.top + 4, node.bounds.left + 21, node.bounds.top + 20);
        if (Presentation::IconPainter::Draw(renderer, node.iconId, iconRect, content, 1)) {
            textX = node.bounds.left + 25;
        }
    }
    renderer.DrawText(textX, node.bounds.top + 6, node.label, content, 2);
}

void WorkspaceRuntime::RenderMenuHeader(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    const bool active = !m_openMenu.empty() && node.payload == m_openMenu;
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (hovered && !disabled ? Color(48, 52, 56) : Color(27, 29, 31)));
    renderer.DrawText(node.bounds.left + 5, node.bounds.top + 5, node.label, disabled ? Color(102, 108, 112) : Color(232, 236, 238), 2);
}

void WorkspaceRuntime::RenderMenuItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, hovered && !disabled ? Color(82, 154, 209) : Color(32, 34, 36));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      Color(52, 55, 58, 90));
    const String left = LeftOfSeparator(node.label);
    const String right = node.shortcut.empty() ? RightOfSeparator(node.label) : node.shortcut;
    const Color text = disabled ? Color(126, 132, 138) : Color(224, 228, 232);
    const Rect box(node.bounds.left + 8, node.bounds.top + 8, node.bounds.left + 18, node.bounds.top + 18);
    renderer.StrokeRect(box, disabled ? Color(84, 88, 92) : Color(178, 186, 194), 1);
    if (node.checked) {
        renderer.FillRect(Rect(box.left + 2, box.top + 2, box.right - 2, box.bottom - 2),
                          disabled ? Color(90, 96, 102) : Color(0, 120, 215));
        renderer.DrawLine(box.left + 3, box.top + 5, box.left + 5, box.bottom - 3, Color(245, 250, 255), 1);
        renderer.DrawLine(box.left + 5, box.bottom - 3, box.right - 2, box.top + 2, Color(245, 250, 255), 1);
    }
    renderer.DrawText(node.bounds.left + 34, node.bounds.top + 6, left, text, 2);
    if (!right.empty()) {
        renderer.DrawText(node.bounds.right - 104, node.bounds.top + 6, right, disabled ? Color(92, 98, 104) : Color(190, 200, 208), 2);
    }
    if (node.hasSubmenu) {
        renderer.DrawText(node.bounds.right - 18, node.bounds.top + 6, L">", disabled ? Color(92, 98, 104) : Color(190, 200, 208), 2);
    }
}

void WorkspaceRuntime::RenderLayerItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    auto* lm = m_canvas.GetLayerManager();
    auto layer = lm ? lm->GetLayer(static_cast<size_t>(node.userData)) : nullptr;
    const bool active = lm && node.userData == lm->GetActiveLayerIndex();
    Color fill = active ? Color(74, 101, 126) : (node.id == m_scene.GetHover() ? Color(63, 66, 69) : Color(50, 52, 54));
    renderer.FillRect(node.bounds, fill);
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      active ? Color(134, 185, 226, 120) : Color(92, 98, 104, 100));
    renderer.StrokeRect(node.bounds, active ? Color(128, 180, 222) : Color(72, 76, 80), 1);
    const Rect thumb(node.bounds.left + 8, node.bounds.top + 6, node.bounds.left + 48, node.bounds.bottom - 6);
    renderer.DrawCheckerboard(thumb, 5, Color(196, 196, 196), Color(230, 230, 230));
    renderer.StrokeRect(thumb, Color(30, 32, 35), 1);
    renderer.DrawText(node.bounds.left + 58, node.bounds.top + 7, node.label, Color(232, 236, 239), 2);
    if (layer) {
        std::wstringstream meta;
        meta << static_cast<int>((layer->GetOpacity() * 100) / 255) << L"% " << BlendName(layer->GetBlendMode());
        renderer.DrawText(node.bounds.left + 58, node.bounds.top + 29, meta.str(), Color(176, 184, 190), 2);
        renderer.DrawText(node.bounds.right - 90, node.bounds.top + 7, layer->IsVisible() ? L"VIS" : L"HID", Color(170, 180, 188), 2);
        if (layer->IsLocked()) renderer.DrawText(node.bounds.right - 44, node.bounds.top + 7, L"LOCK", Color(238, 190, 90), 2);
        if (layer->IsProtectAlpha()) renderer.DrawText(node.bounds.right - 44, node.bounds.top + 29, L"A", Color(140, 210, 255), 2);
    }
}

void WorkspaceRuntime::RenderSwatch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const Color swatch = ColorFromPackedRgba(node.userData);
    const bool active = swatch.r == m_canvas.GetColor().r && swatch.g == m_canvas.GetColor().g && swatch.b == m_canvas.GetColor().b && swatch.a == m_canvas.GetColor().a;
    if (swatch.a == 0) {
        renderer.DrawCheckerboard(node.bounds, 5, Color(235, 235, 235), Color(180, 180, 180));
    } else {
        renderer.FillRect(node.bounds, swatch);
    }
    renderer.StrokeRect(node.bounds, active ? Color(245, 250, 255) : Color(112, 122, 132), active ? 2 : 1);
}

void WorkspaceRuntime::RenderSlider(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const AppCommand command = static_cast<AppCommand>(node.command);
    uint16_t value = 0;
    if (command == AppCommand::SetLayerOpacity) {
        value = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(node.userData) * 100 / 255, 0, 100));
    } else {
        value = BrushParamValueFromPacked(node.userData);
    }
    const bool hovered = node.id == m_scene.GetHover() || node.id == m_scene.GetCapture();
    renderer.DrawText(node.bounds.left, node.bounds.top + 2, node.label, Color(204, 210, 214), 2);
    const Rect track(node.bounds.left + 86, node.bounds.top + 10, node.bounds.right - 46, node.bounds.top + 16);
    renderer.FillRect(track, Color(25, 27, 29));
    const int knobX = track.left + (track.Width() * std::clamp<int>(value, 0, 100)) / 100;
    renderer.FillRect(Rect(track.left, track.top, knobX, track.bottom), hovered ? Color(42, 157, 244) : Color(0, 120, 215));
    renderer.FillRect(Rect(knobX - 3, track.top - 5, knobX + 3, track.bottom + 5), Color(230, 234, 237));
    std::wstringstream valueText;
    if (command == AppCommand::SetBrushParam && BrushParamFromPacked(node.userData) == BrushParamId::Size) {
        valueText << std::max<uint16_t>(1, value);
    } else {
        valueText << value << L"%";
    }
    renderer.DrawText(node.bounds.right - 38, node.bounds.top + 2, valueText.str(), Color(198, 204, 208), 2);
}

void WorkspaceRuntime::RenderSearchBox(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool focused = node.id == m_scene.GetFocus();
    const bool hovered = node.id == m_scene.GetHover();
    renderer.FillRect(node.bounds, Color(29, 32, 35));
    renderer.StrokeRect(node.bounds,
                        focused ? Color(0, 120, 215)
                                : hovered ? Color(114, 126, 138)
                                          : Color(82, 88, 94),
                        focused ? 2 : 1);
    const String value = node.label.empty() ? L"0" : node.label;
    renderer.DrawText(node.bounds.left + 10, node.bounds.top + 10, value, Color(236, 240, 242), 2);
}

void WorkspaceRuntime::RenderColorField(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const Hsv hsv = RgbToHsv(m_canvas.GetColor());
    renderer.DrawHsvSquare(node.bounds, hsv.h);
    renderer.StrokeRect(node.bounds, Color(26, 28, 30), 2);
    const int cx = node.bounds.left + static_cast<int>(hsv.s * std::max(1, node.bounds.Width() - 1));
    const int cy = node.bounds.top + static_cast<int>((1.0f - hsv.v) * std::max(1, node.bounds.Height() - 1));
    renderer.StrokeRect(Rect(cx - 5, cy - 5, cx + 5, cy + 5), Color(255, 255, 255), 1);
    renderer.StrokeRect(Rect(cx - 6, cy - 6, cx + 6, cy + 6), Color(30, 30, 30), 1);
}

void WorkspaceRuntime::RenderHueStrip(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const Hsv hsv = RgbToHsv(m_canvas.GetColor());
    renderer.DrawHueStrip(node.bounds);
    renderer.StrokeRect(node.bounds, Color(26, 28, 30), 1);
    const int y = node.bounds.top + static_cast<int>((hsv.h / 360.0f) * std::max(1, node.bounds.Height() - 1));
    renderer.StrokeRect(Rect(node.bounds.left - 3, y - 4, node.bounds.right + 3, y + 4), Color(235, 240, 245), 1);
}

void WorkspaceRuntime::RenderBrushPresetItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    const uint16_t size = BrushPresetSizeFromPacked(node.userData);
    const auto tip = static_cast<Render::BrushTipType>(BrushPresetTipFromPacked(node.userData));
    const bool active = static_cast<int>(m_canvas.GetBrushSize()) == size && m_canvas.GetBrushTip() == tip;
    renderer.FillRect(node.bounds, active ? Color(82, 154, 209) : (hovered ? Color(68, 72, 75) : Color(55, 58, 60)));
    renderer.StrokeRect(node.bounds, active ? Color(146, 205, 255) : Color(82, 86, 90), 1);
    Color accent = Color(0, 120, 215);
    if (!node.payload.empty()) {
        try {
            accent = ColorFromPackedRgba(std::stoull(node.payload));
        } catch (...) {
            accent = Color(0, 120, 215);
        }
    }
    renderer.FillRect(Rect(node.bounds.left + 6, node.bounds.top + 6, node.bounds.left + 16, node.bounds.bottom - 6), accent);
    renderer.FillCircle(node.bounds.left + 30, node.bounds.top + node.bounds.Height() / 2, std::max(2, static_cast<int>(size / 8)), Color(235, 238, 240));
    renderer.DrawText(node.bounds.left + 48, node.bounds.top + 6, node.label, Color(232, 236, 238), 2);
}

void WorkspaceRuntime::RenderBrushSizeChip(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const uint16_t size = BrushParamValueFromPacked(node.userData);
    const bool active = std::abs(m_canvas.GetBrushSize() - static_cast<float>(size)) < 0.5f;
    const bool hovered = node.id == m_scene.GetHover();
    renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (hovered ? Color(62, 66, 70) : Color(47, 50, 53)));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      active ? Color(128, 204, 255, 130) : Color(96, 102, 108, 100));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.bottom - 1, node.bounds.right, node.bounds.bottom),
                      Color(18, 20, 22, 170));
    renderer.StrokeRect(node.bounds, active ? Color(138, 202, 255) : Color(72, 76, 80), 1);
    renderer.FillCircle(node.bounds.left + node.bounds.Width() / 2, node.bounds.top + 14, std::clamp<int>(size / 12, 2, 10), Color(238, 241, 244));
    renderer.DrawText(node.bounds.left + 8, node.bounds.bottom - 16, node.label, Color(204, 210, 214), 2);
}

void WorkspaceRuntime::RenderCheckBox(Presentation::SoftRenderer&, const CoreUI::UiNode&, bool) {}

void WorkspaceRuntime::RenderPanel(Presentation::SoftRenderer& renderer, const Rect& rect, const String& title) {
    renderer.FillVerticalGradient(rect, Color(48, 50, 52), Color(39, 41, 43));
    renderer.FillVerticalGradient(Rect(rect.left, rect.top, rect.right, rect.top + 24), Color(40, 43, 46), Color(29, 31, 34));
    renderer.StrokeRect(rect, Color(74, 78, 82), 1);
    renderer.FillRect(Rect(rect.left + 1, rect.top + 1, rect.right - 1, rect.top + 2), Color(95, 101, 107, 120));
    renderer.FillRect(Rect(rect.left, rect.top + 24, rect.right, rect.top + 25), Color(0, 120, 215, 120));
    renderer.DrawText(rect.left + 8, rect.top + 6, title, Color(220, 225, 228), 2);
    renderer.DrawText(rect.right - 40, rect.top + 6, L"[] x", Color(150, 158, 164), 2);
}

void WorkspaceRuntime::ApplySliderAtPoint(const CoreUI::UiNode& node, const Point& position) {
    const int trackLeft = node.bounds.left + 86;
    const int trackRight = node.bounds.right - 46;
    const int width = std::max(1, trackRight - trackLeft);
    const int clampedX = std::clamp(position.x, trackLeft, trackRight);
    const uint16_t value = static_cast<uint16_t>(((clampedX - trackLeft) * 100) / width);
    const AppCommand command = static_cast<AppCommand>(node.command);
    if (command == AppCommand::SetBrushParam) {
        DispatchCommand(AppCommand::SetBrushParam, PackBrushParam(BrushParamFromPacked(node.userData), value), node.payload);
    } else if (command == AppCommand::SetLayerOpacity) {
        DispatchCommand(AppCommand::SetLayerOpacity, static_cast<uint64_t>((value * 255) / 100), node.payload);
    }
}

void WorkspaceRuntime::ApplyColorFieldAtPoint(const CoreUI::UiNode& node, const Point& position) {
    const Hsv current = RgbToHsv(m_canvas.GetColor());
    const float s = std::clamp((position.x - node.bounds.left) / static_cast<float>(std::max(1, node.bounds.Width() - 1)), 0.0f, 1.0f);
    const float v = 1.0f - std::clamp((position.y - node.bounds.top) / static_cast<float>(std::max(1, node.bounds.Height() - 1)), 0.0f, 1.0f);
    OnSetColor(HsvToRgb(current.h, s, v, m_canvas.GetColor().a));
}

void WorkspaceRuntime::ApplyHueStripAtPoint(const CoreUI::UiNode& node, const Point& position) {
    Hsv current = RgbToHsv(m_canvas.GetColor());
    current.h = std::clamp((position.y - node.bounds.top) / static_cast<float>(std::max(1, node.bounds.Height() - 1)), 0.0f, 1.0f) * 360.0f;
    if (current.s <= 0.01f) current.s = 1.0f;
    if (current.v <= 0.01f) current.v = 1.0f;
    OnSetColor(HsvToRgb(current.h, current.s, current.v, m_canvas.GetColor().a));
}

bool WorkspaceRuntime::SaveCurrentProjectInteractive(bool saveAs) {
    return SaveProject(saveAs);
}

bool WorkspaceRuntime::SaveProject(bool saveAs) {
    if (!m_session.HasProject()) {
        SetStatus(L"NO PROJECT TO SAVE");
        return false;
    }
    bool ok = false;
    if (saveAs || m_session.GetCurrentFilePath().empty()) {
        auto* dialogs = CoreServices::GetFileDialogService();
        if (!dialogs) return false;
        String path = m_session.GetCurrentFilePath();
        if (!dialogs->PickSaveProjectPath(path)) return false;
        ok = m_session.SaveProjectAs(path, m_canvas.GetLayerManager());
    } else {
        ok = m_session.SaveProject(m_canvas.GetLayerManager());
    }
    SetStatus(ok ? L"SAVED" : L"SAVE FAILED");
    return ok;
}

void WorkspaceRuntime::ExportPng() {
    if (!m_session.HasProject()) {
        SetStatus(L"NO PROJECT TO EXPORT");
        return;
    }
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) return;
    String path;
    if (!dialogs->PickExportImagePath(path)) return;
    SetStatus(m_session.ExportPNG(path, m_canvas.GetLayerManager()) ? L"EXPORTED" : L"EXPORT FAILED");
}

void WorkspaceRuntime::SwitchTool(ToolType tool) {
    m_canvas.SetTool(tool);
    SetStatus(ToolName(tool));
}

void WorkspaceRuntime::SetStatus(const String& status) {
    m_status = status;
    TouchWorkspace();
}

void WorkspaceRuntime::MarkProjectDirty() {
    m_session.MarkDirty();
}

bool WorkspaceRuntime::ConfirmAbandonUnsavedChanges(const String& actionLabel) {
    if (!m_session.HasProject() || !m_session.HasUnsavedChanges()) {
        return true;
    }
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) {
        SetStatus(L"UNSAVED CHANGES");
        return false;
    }
    switch (dialogs->PromptUnsavedChanges(actionLabel)) {
        case UnsavedChangesChoice::Save:
            if (SaveProject(false)) {
                return true;
            }
            SetStatus(L"SAVE CANCELLED");
            return false;
        case UnsavedChangesChoice::Discard:
            return true;
        case UnsavedChangesChoice::Cancel:
        default:
            break;
    }
    SetStatus(L"CANCELLED");
    return false;
}

bool WorkspaceRuntime::HandleCanvasSizeKey(const Input::KeyEvent& event) {
    if (event.keyCode == 0x1B) {
        OnCloseModal();
        return true;
    }
    if (event.keyCode == 0x0D) {
        OnCreateCanvasFromForm();
        return true;
    }
    if (event.keyCode == 0x09) {
        m_canvasSizeField = m_canvasSizeField == CanvasSizeField::Height ? CanvasSizeField::Width : CanvasSizeField::Height;
        TouchWorkspace();
        return true;
    }

    if (m_canvasSizeField == CanvasSizeField::None) {
        return false;
    }

    auto* value = m_canvasSizeField == CanvasSizeField::Width ? &m_canvasResizeWidth : &m_canvasResizeHeight;
    if (event.keyCode >= '0' && event.keyCode <= '9') {
        const uint32_t digit = static_cast<uint32_t>(event.keyCode - '0');
        *value = std::min<uint32_t>(*value * 10 + digit, 65535u);
        TouchWorkspace();
        return true;
    }
    if (event.keyCode == 0x08) {
        *value /= 10;
        TouchWorkspace();
        return true;
    }
    if (event.keyCode == 0x26) {
        *value = std::min<uint32_t>(*value + 1, 65535u);
        TouchWorkspace();
        return true;
    }
    if (event.keyCode == 0x28) {
        *value = *value > 1 ? *value - 1 : 1;
        TouchWorkspace();
        return true;
    }
    return false;
}

void WorkspaceRuntime::TouchWorkspace() {
    BuildScene();
    MarkDirty(FullRect());
}

void WorkspaceRuntime::OnGoHome() { m_wantsProgramManager = true; TouchWorkspace(); }
void WorkspaceRuntime::OnShowNewCanvasModal() { m_wantsProgramManager = true; TouchWorkspace(); }
void WorkspaceRuntime::OnShowSettingsModal() {
    m_wantsProgramManager = true;
    m_wantsProgramManagerSettings = true;
    TouchWorkspace();
}
void WorkspaceRuntime::OnCreateCanvasFromForm() {
    if (m_modals.Current() != ModalKind::CanvasSize || !m_session.HasProject()) {
        return;
    }
    const uint32_t width = std::clamp<uint32_t>(m_canvasResizeWidth, 1, 65535);
    const uint32_t height = std::clamp<uint32_t>(m_canvasResizeHeight, 1, 65535);
    const uint32_t anchorIndex = static_cast<uint32_t>(m_canvasAnchor);
    const uint32_t anchorX = anchorIndex % 3;
    const uint32_t anchorY = anchorIndex / 3;
    if (m_canvas.ResizeCanvas(width, height, anchorX, anchorY)) {
        MarkProjectDirty();
        m_modals.Close();
        m_canvasSizeField = CanvasSizeField::None;
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
    std::swap(m_canvasResizeWidth, m_canvasResizeHeight);
    TouchWorkspace();
}
void WorkspaceRuntime::OnSetCanvasAnchor(uint32_t anchor) {
    m_canvasAnchor = static_cast<CanvasAnchor>(std::min<uint32_t>(anchor, 8));
    TouchWorkspace();
}
void WorkspaceRuntime::OnCreateCanvasPreset(uint32_t width, uint32_t height) {
    if (m_modals.Current() == ModalKind::CanvasSize) {
        m_canvasResizeWidth = std::clamp<uint32_t>(width, 1, 65535);
        m_canvasResizeHeight = std::clamp<uint32_t>(height, 1, 65535);
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
    m_canvasSizeField = CanvasSizeField::None;
    m_modals.Close();
    TouchWorkspace();
}
void WorkspaceRuntime::OnOpenProject() {
    if (!ConfirmAbandonUnsavedChanges(L"opening another project")) return;
    auto* dialogs = CoreServices::GetFileDialogService();
    if (!dialogs) return;
    String path;
    if (!dialogs->PickOpenProjectPath(path)) return;
    if (path.empty()) return;
    if (m_session.OpenProject(path, m_canvas.GetLayerManager())) {
        m_canvas.AttachLoadedProject(m_session.GetProject());
        m_canvas.ResizeViewport(CanvasRect());
        SetStatus(L"OPENED");
    } else {
        SetStatus(L"OPEN FAILED");
    }
}

void WorkspaceRuntime::OnOpenRecentProject(const String& path) {
    if (path.empty()) return;
    if (!ConfirmAbandonUnsavedChanges(L"opening another project")) return;
    if (m_session.OpenProject(path, m_canvas.GetLayerManager())) {
        m_canvas.AttachLoadedProject(m_session.GetProject());
        m_canvas.ResizeViewport(CanvasRect());
        SetStatus(L"OPENED");
    } else {
        SetStatus(L"OPEN FAILED");
    }
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
        m_canvasResizeWidth = project->GetCanvas().widthPx;
        m_canvasResizeHeight = project->GetCanvas().heightPx;
    }
    m_canvasAnchor = CanvasAnchor::Center;
    m_canvasSizeField = CanvasSizeField::Width;
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
void WorkspaceRuntime::OnSetBlendMode(uint32_t mode) { m_canvas.SetActiveLayerBlendMode(static_cast<BlendMode>(mode)); MarkProjectDirty(); SetStatus(BlendName(static_cast<BlendMode>(mode))); }
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
    m_canvas.TogglePanel(panel);
    SaveWorkspaceUiSettings();
    SetStatus(L"PANEL TOGGLED");
}
void WorkspaceRuntime::OnToggleLeftSidebar() {
    m_canvas.ToggleLeftSidebar();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    TouchWorkspace();
    SetStatus(m_canvas.IsLeftSidebarExpanded() ? L"LEFT PANEL OPEN" : L"LEFT PANEL CLOSED");
}
void WorkspaceRuntime::OnToggleRightSidebar() {
    m_canvas.ToggleRightSidebar();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    TouchWorkspace();
    SetStatus(m_canvas.IsRightSidebarExpanded() ? L"RIGHT PANEL OPEN" : L"RIGHT PANEL CLOSED");
}
void WorkspaceRuntime::OnInitializeLayout() {
    m_canvas.InitializeWorkspaceLayout();
    m_canvas.ResizeViewport(CanvasRect());
    SaveWorkspaceUiSettings();
    SetStatus(L"LAYOUT INITIALIZED");
}
void WorkspaceRuntime::OnShowUnavailable() { SetStatus(L"NOT AVAILABLE IN MVP"); }
void WorkspaceRuntime::OnCloseWorkspace() {
    if (!ConfirmAbandonUnsavedChanges(L"closing the current project")) return;
    m_canvas.CloseProject();
    m_session.AttachProject(nullptr);
    m_wantsProgramManager = true;
    SetStatus(L"NO PROJECT");
}

} // namespace CloverPic::Core
