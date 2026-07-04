#include "Core/App/ProjectManagerRuntime.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/App/AppSceneBuilder.h"
#include "Core/Presentation/SoftRenderer.h"
#include "Core/Services/CoreServices.h"
#include "Core/Text/CoreTextEngine.h"
#include <algorithm>
#include <cwctype>

namespace CloverPic::Core {

namespace {

bool IsHomeTextKey(uint32_t keyCode) {
    return (keyCode >= 'A' && keyCode <= 'Z') ||
           (keyCode >= '0' && keyCode <= '9') ||
           keyCode == 0x20 || keyCode == 0xBD || keyCode == 0xBE;
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

Presentation::UiNodeId AddPmNode(CoreUI::UiScene& scene,
                                 CoreUI::UiNodeType type,
                                 const Rect& bounds,
                                 String label,
                                 AppCommand command = AppCommand::None,
                                 uint64_t userData = 0,
                                 String payload = L"",
                                 int z = 20,
                                 uint32_t flags = Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
                                 Presentation::IconId iconId = Presentation::IconId::None,
                                 CoreUI::IconPlacement placement = CoreUI::IconPlacement::None) {
    CoreUI::UiNode node;
    node.type = type;
    node.bounds = bounds;
    node.label = std::move(label);
    node.accessibilityLabel = node.label;
    node.command = static_cast<uint32_t>(command);
    node.userData = userData;
    node.payload = std::move(payload);
    node.zOrder = z;
    node.flags = flags;
    node.iconId = iconId;
    node.iconPlacement = placement;
    return scene.AddNode(std::move(node));
}

} // namespace

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
    if (m_page == Page::NewImage) {
        RenderNewImage(renderer);
    } else if (m_page == Page::Settings) {
        RenderSettings(renderer);
    } else {
        RenderHome(renderer);
    }
    outDirtyRects = m_scheduler.ConsumeDirtyRects(nowMs, FullRect());
    if (outDirtyRects.empty()) outDirtyRects.push_back(FullRect());
    return m_frame;
}

void ProjectManagerRuntime::HandlePointer(const Input::PointerEvent& event) {
    if (m_frame.IsEmpty()) return;
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
            if (hit->payload == ActiveFieldPayload(ActiveField::Width)) m_activeField = ActiveField::Width;
            else if (hit->payload == ActiveFieldPayload(ActiveField::Height)) m_activeField = ActiveField::Height;
            else if (hit->payload == ActiveFieldPayload(ActiveField::Dpi)) m_activeField = ActiveField::Dpi;
            else m_activeField = ActiveField::Search;
        } else {
            m_activeField = ActiveField::None;
        }
        MarkDirty(FullRect());
    } else if (event.action == Input::PointerAction::Up) {
        if (hit && hit->id == m_scene.GetCapture()) {
            DispatchCommand(static_cast<AppCommand>(hit->command), hit->userData, hit->payload);
        }
        m_scene.SetCapture(0);
        MarkDirty(FullRect());
    }
}

void ProjectManagerRuntime::HandleKey(const Input::KeyEvent& event) {
    if (event.action != Input::KeyAction::Down) return;
    if (HandleFormKey(event)) return;
    if (HandleSearchKey(event)) return;
    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    if (event.keyCode == 0x1B) {
        if (m_page != Page::Start) OnCloseModal();
        else m_wantsQuit = true;
    } else if (event.keyCode == 'N' && ctrl) {
        StartNewCanvas(1600, 1000);
    } else if (event.keyCode == 'O' && ctrl) {
        OnOpenProject();
    }
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
    m_page = Page::Settings;
    m_activeField = ActiveField::None;
    m_settingsOpenedFromWorkspace = true;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::BuildScene() {
    if (m_page == Page::NewImage) {
        BuildNewImageScene();
    } else if (m_page == Page::Settings) {
        BuildSettingsScene();
    } else {
        AppSceneBuilder::Build(AppScreen::Home, ModalKind::None, m_viewport, m_frame, m_session, m_dummyCanvas, m_searchQuery, m_scene);
    }
}

void ProjectManagerRuntime::BuildNewImageScene() {
    m_scene.Clear();
    const int margin = std::max(24, m_viewport.width / 34);
    const int navW = std::clamp(m_viewport.width / 4, 220, 280);
    const int left = margin;
    const int top = margin;
    const int contentLeft = left + navW + 28;
    const int contentRight = m_viewport.width - margin;
    const int rowH = 36;
    const int labelW = 148;
    const int fieldW = std::clamp((contentRight - contentLeft) / 2, 220, 360);
    const int fieldLeft = contentLeft + labelW;

    AddPmNode(m_scene, CoreUI::UiNodeType::ActionItem, Rect(left, top + 70, left + navW, top + 114),
              L"RECENT", AppCommand::CloseModal);
    AddPmNode(m_scene, CoreUI::UiNodeType::ActionItem, Rect(left, top + 122, left + navW, top + 166),
              L"CREATE NEW IMAGE", AppCommand::ShowNewCanvasModal);
    AddPmNode(m_scene, CoreUI::UiNodeType::ActionItem, Rect(left, top + 174, left + navW, top + 218),
              L"SETTINGS", AppCommand::ShowSettingsModal);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(contentRight - 36, top, contentRight, top + 30),
              L"X", AppCommand::CloseModal, 0, L"", 60, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
              Presentation::IconId::X, CoreUI::IconPlacement::Center);

    auto field = [&](int y, const String& label, const String& value, ActiveField fieldId) {
        AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(contentLeft, y + 8, contentLeft + labelW, y + rowH),
                  label, AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
        AddPmNode(m_scene, CoreUI::UiNodeType::SearchBox, Rect(fieldLeft, y, fieldLeft + fieldW, y + rowH),
                  value, AppCommand::None, 0, ActiveFieldPayload(fieldId), 30);
    };

    int y = top + 96;
    field(y, L"Width", std::to_wstring(m_formWidth), ActiveField::Width);
    y += rowH + 10;
    field(y, L"Height", std::to_wstring(m_formHeight), ActiveField::Height);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(fieldLeft + fieldW + 12, y - rowH - 10, fieldLeft + fieldW + 170, y + rowH),
              L"Swap Orientation", AppCommand::SwapCanvasOrientation);
    y += rowH + 10;
    field(y, L"Resolution", std::to_wstring(m_formDpi), ActiveField::Dpi);
    AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(fieldLeft + fieldW + 12, y + 8, fieldLeft + fieldW + 64, y + rowH),
              L"dpi", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += rowH + 14;

    AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(contentLeft, y + 8, contentLeft + labelW, y + rowH),
              L"Background", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(fieldLeft, y, fieldLeft + fieldW, y + rowH),
              m_formTransparent ? L"Transparent" : L"White paper", AppCommand::ToggleCanvasTransparency);
    y += rowH + 14;

    AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(contentLeft, y + 8, contentLeft + labelW, y + rowH),
              L"Initial Layer", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(fieldLeft, y, fieldLeft + fieldW, y + rowH),
              m_formTransparent ? L"Transparent Layer" : L"Color Layer", AppCommand::None, 0,
              L"", 30, Presentation::UiNodeVisible);
    y += rowH + 18;

    AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(contentLeft, y, contentLeft + 260, y + 24),
              L"RGB Profile", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += 28;
    const size_t profileLimit = std::min<size_t>(m_profiles.size(), 5);
    for (size_t i = 0; i < profileLimit; ++i) {
        AddPmNode(m_scene, CoreUI::UiNodeType::Button,
                  Rect(fieldLeft, y + static_cast<int>(i) * 32, contentRight - 12, y + static_cast<int>(i) * 32 + 28),
                  (i == m_selectedRgbProfile ? L"* " : L"  ") + ProfileLabel(i),
                  AppCommand::SelectRgbProfile, i);
    }
    y += static_cast<int>(profileLimit) * 32 + 16;

    AddPmNode(m_scene, CoreUI::UiNodeType::Text, Rect(contentLeft, y, contentLeft + 260, y + 24),
              L"CMYK Profile", AppCommand::None, 0, L"", 8, Presentation::UiNodeVisible);
    y += 28;
    for (size_t i = 0; i < profileLimit; ++i) {
        AddPmNode(m_scene, CoreUI::UiNodeType::Button,
                  Rect(fieldLeft, y + static_cast<int>(i) * 32, contentRight - 12, y + static_cast<int>(i) * 32 + 28),
                  (i == m_selectedCmykProfile ? L"* " : L"  ") + ProfileLabel(i),
                  AppCommand::SelectCmykProfile, i);
    }

    const int footerY = m_viewport.height - margin - 48;
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(contentRight - 350, footerY, contentRight - 236, footerY + 38),
              L"Cancel", AppCommand::CloseModal);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(contentRight - 224, footerY, contentRight, footerY + 38),
              L"Create Image", AppCommand::CreateCanvasFromForm);
}

void ProjectManagerRuntime::BuildSettingsScene() {
    m_scene.Clear();
    const int margin = 18;
    const int panelLeft = 0;
    const int panelTop = 0;
    const int panelRight = m_viewport.width;
    const int panelBottom = m_viewport.height;
    const int navW = 220;
    const wchar_t* categories[] = { L"General", L"Canvas", L"Color", L"Performance", L"Input", L"Files" };

    for (size_t i = 0; i < 6; ++i) {
        AddPmNode(m_scene, CoreUI::UiNodeType::ActionItem,
                  Rect(panelLeft + 16, panelTop + 70 + static_cast<int>(i) * 46,
                       panelLeft + navW - 14, panelTop + 110 + static_cast<int>(i) * 46),
                  categories[i], AppCommand::SetSettingsCategory, i);
    }

    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(panelRight - 36, panelTop + 12, panelRight - 8, panelTop + 40),
              L"X", AppCommand::CloseModal, 0, L"", 60, Presentation::UiNodeVisible | Presentation::UiNodeInteractive,
              Presentation::IconId::X, CoreUI::IconPlacement::Center);

    const int footerY = panelBottom - 52;
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(panelRight - margin - 352, footerY, panelRight - margin - 246, footerY + 36),
              L"Apply", AppCommand::ApplySettings);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(panelRight - margin - 238, footerY, panelRight - margin - 132, footerY + 36),
              L"Cancel", AppCommand::CloseModal);
    AddPmNode(m_scene, CoreUI::UiNodeType::Button, Rect(panelRight - margin - 124, footerY, panelRight - margin, footerY + 36),
              L"Save", AppCommand::SaveSettings);
}

static void FocusSearchBox(CoreUI::UiScene& scene) {
    for (const auto& node : scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::SearchBox) {
            scene.SetFocus(node.id);
            return;
        }
    }
}

void ProjectManagerRuntime::MarkDirty(const Rect& rect) {
    m_scheduler.Invalidate(rect);
}

void ProjectManagerRuntime::DispatchCommand(AppCommand command, uint64_t userData, const String& payload) {
    m_commands.Dispatch(*this, command, userData, payload);
}

bool ProjectManagerRuntime::HandleSearchKey(const Input::KeyEvent& event) {
    if (m_page != Page::Start) return false;
    const bool ctrl = (event.modifiers & Input::ModifierControl) != 0;
    const bool shift = (event.modifiers & Input::ModifierShift) != 0;
    const auto* focused = m_scene.Find(m_scene.GetFocus());
    const bool searchFocused = focused && focused->type == CoreUI::UiNodeType::SearchBox;
    if (event.keyCode == 0x1B && !m_searchQuery.empty()) {
        m_searchQuery.clear();
        BuildScene();
        FocusSearchBox(m_scene);
        MarkDirty(FullRect());
        return true;
    }
    if (!searchFocused || ctrl) return false;
    if (event.keyCode == 0x08) {
        if (!m_searchQuery.empty()) {
            m_searchQuery.pop_back();
            BuildScene();
            FocusSearchBox(m_scene);
            MarkDirty(FullRect());
        }
        return true;
    }
    if (IsHomeTextKey(event.keyCode) && m_searchQuery.size() < 48) {
        const wchar_t ch = CharFromKey(event.keyCode, shift);
        if (ch != 0) {
            m_searchQuery.push_back(ch);
            BuildScene();
            FocusSearchBox(m_scene);
            MarkDirty(FullRect());
            return true;
        }
    }
    return false;
}

bool ProjectManagerRuntime::HandleFormKey(const Input::KeyEvent& event) {
    if (m_page != Page::NewImage) return false;
    const auto* focused = m_scene.Find(m_scene.GetFocus());
    if (!focused || focused->type != CoreUI::UiNodeType::SearchBox) return false;

    ActiveField field = ActiveField::None;
    if (focused->payload == ActiveFieldPayload(ActiveField::Width)) field = ActiveField::Width;
    else if (focused->payload == ActiveFieldPayload(ActiveField::Height)) field = ActiveField::Height;
    else if (focused->payload == ActiveFieldPayload(ActiveField::Dpi)) field = ActiveField::Dpi;
    if (field == ActiveField::None) return false;
    m_activeField = field;

    auto mutateNumber = [&](uint32_t& value, uint32_t minValue, uint32_t maxValue) {
        String text = std::to_wstring(value);
        if (event.keyCode == 0x08) {
            if (!text.empty()) text.pop_back();
        } else if (event.keyCode >= '0' && event.keyCode <= '9') {
            if (text.size() < 6) text.push_back(static_cast<wchar_t>(event.keyCode));
        } else {
            return false;
        }
        uint32_t next = minValue;
        if (!text.empty()) {
            try {
                next = static_cast<uint32_t>(std::stoul(text));
            } catch (...) {
                next = minValue;
            }
        }
        value = std::clamp(next, minValue, maxValue);
        BuildScene();
        for (const auto& node : m_scene.Nodes()) {
            if (node.payload == ActiveFieldPayload(field)) {
                m_scene.SetFocus(node.id);
                break;
            }
        }
        MarkDirty(FullRect());
        return true;
    };

    if (field == ActiveField::Width) return mutateNumber(m_formWidth, 16, 65535);
    if (field == ActiveField::Height) return mutateNumber(m_formHeight, 16, 65535);
    if (field == ActiveField::Dpi) return mutateNumber(m_formDpi, 1, 2400);
    return false;
}

void ProjectManagerRuntime::RenderHome(Presentation::SoftRenderer& renderer) {
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
        if (node.type == CoreUI::UiNodeType::ActionItem) RenderHomeAction(renderer, node);
        else if (node.type == CoreUI::UiNodeType::RecentItem) RenderHomeRecent(renderer, node);
        else if (node.type == CoreUI::UiNodeType::SearchBox) RenderHomeSearch(renderer, node);
        else if (node.type == CoreUI::UiNodeType::Tile) RenderHomeTile(renderer, node);
        else if (node.type == CoreUI::UiNodeType::Text) renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(118, 118, 118), 2);
        else if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node);
    }
}

void ProjectManagerRuntime::RenderNewImage(Presentation::SoftRenderer& renderer) {
    renderer.Clear(Color(18, 20, 22));
    renderer.FillRect(Rect(0, 0, m_viewport.width, 6), Color(0, 120, 215));
    renderer.FillVerticalGradient(Rect(0, 6, m_viewport.width, m_viewport.height), Color(32, 35, 38), Color(18, 20, 22));

    const int margin = std::max(24, m_viewport.width / 34);
    const int navW = std::clamp(m_viewport.width / 4, 220, 280);
    const int left = margin;
    const int top = margin;
    const int contentLeft = left + navW + 28;
    const int contentRight = m_viewport.width - margin;
    const int contentBottom = m_viewport.height - margin;

    renderer.FillRect(Rect(left, top, left + navW, contentBottom), Color(24, 26, 28));
    renderer.StrokeRect(Rect(left, top, left + navW, contentBottom), Color(68, 74, 80), 1);
    renderer.FillCircle(left + 28, top + 30, 16, Color(245, 248, 250));
    renderer.DrawText(left + 56, top + 20, L"CloverPic", Color(238, 242, 245), 2);
    renderer.DrawText(left + 56, top + 42, L"Program Manager", Color(136, 146, 154), 1);

    renderer.FillRect(Rect(contentLeft - 18, top, contentRight, contentBottom), Color(35, 38, 41));
    renderer.StrokeRect(Rect(contentLeft - 18, top, contentRight, contentBottom), Color(78, 84, 90), 1);
    renderer.FillVerticalGradient(Rect(contentLeft - 18, top, contentRight, top + 58), Color(43, 47, 51), Color(31, 34, 37));
    renderer.DrawText(contentLeft, top + 18, L"Create New Image", Color(244, 247, 249), 3);
    renderer.DrawText(contentLeft, top + 50, L"10bit RGBA document, profile-aware project container", Color(150, 160, 168), 1);

    const int previewLeft = contentRight - 190;
    const int previewTop = top + 88;
    renderer.DrawCheckerboard(Rect(previewLeft, previewTop, contentRight - 22, previewTop + 120), 10,
                              Color(68, 72, 76), Color(54, 58, 62));
    const float aspect = m_formHeight == 0 ? 1.0f : static_cast<float>(m_formWidth) / m_formHeight;
    int paperW = 130;
    int paperH = static_cast<int>(paperW / std::max(0.2f, aspect));
    if (paperH > 96) {
        paperH = 96;
        paperW = static_cast<int>(paperH * aspect);
    }
    const int paperL = previewLeft + ((contentRight - 22 - previewLeft) - paperW) / 2;
    const int paperT = previewTop + (120 - paperH) / 2;
    renderer.FillRect(Rect(paperL, paperT, paperL + paperW, paperT + paperH),
                      m_formTransparent ? Color(250, 250, 250, 190) : m_formBackground);
    renderer.StrokeRect(Rect(paperL, paperT, paperL + paperW, paperT + paperH), Color(0, 120, 215), 2);

    for (const auto& node : m_scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) RenderHomeAction(renderer, node);
        else if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node);
        else if (node.type == CoreUI::UiNodeType::SearchBox) {
            const bool focused = node.id == m_scene.GetFocus();
            renderer.FillRect(node.bounds, Color(55, 58, 62));
            renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : Color(90, 96, 102), focused ? 2 : 1);
            renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1), Color(108, 114, 120, 170));
            renderer.DrawText(node.bounds.left + 10, node.bounds.top + 9, node.label, Color(238, 242, 245), 2);
        } else if (node.type == CoreUI::UiNodeType::Text) {
            renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(194, 202, 208), 2);
        }
    }
}

void ProjectManagerRuntime::RenderSettings(Presentation::SoftRenderer& renderer) {
    renderer.Clear(Color(24, 27, 30));
    renderer.FillVerticalGradient(FullRect(), Color(39, 42, 46), Color(20, 22, 25));

    const int panelLeft = 0;
    const int panelTop = 0;
    const int panelRight = m_viewport.width;
    const int panelBottom = m_viewport.height;
    const int navW = 220;
    const int detailLeft = panelLeft + navW;

    renderer.FillRect(Rect(panelLeft, panelTop, panelRight, panelBottom), Color(34, 37, 40));
    renderer.FillRect(Rect(panelLeft, panelTop, panelRight, panelTop + 6), Color(0, 120, 215));
    renderer.FillVerticalGradient(Rect(panelLeft, panelTop + 6, panelRight, panelTop + 58), Color(46, 50, 54), Color(30, 33, 36));
    renderer.FillRect(Rect(panelLeft, panelTop, detailLeft, panelBottom), Color(25, 27, 29));
    renderer.FillRect(Rect(detailLeft, panelTop + 58, detailLeft + 1, panelBottom - 58), Color(70, 76, 82));
    renderer.FillRect(Rect(panelLeft, panelBottom - 58, panelRight, panelBottom - 57), Color(70, 76, 82));
    renderer.DrawText(panelLeft + 20, panelTop + 22, L"Settings", Color(244, 247, 249), 3);

    for (const auto& node : m_scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) {
            const bool active = static_cast<int>(node.userData) == m_settingsCategory;
            renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (node.id == m_scene.GetHover() ? Color(48, 52, 56) : Color(25, 27, 29)));
            renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 4, node.bounds.bottom),
                              active ? Color(126, 201, 255) : Color(56, 62, 68));
            renderer.DrawText(node.bounds.left + 14, node.bounds.top + 12, node.label,
                              active ? Color(255, 255, 255) : Color(210, 216, 220), 2);
        } else if (node.type == CoreUI::UiNodeType::Button) {
            RenderButton(renderer, node);
        }
    }

    const int x = detailLeft + 28;
    int y = panelTop + 82;
    const wchar_t* titles[] = { L"General", L"Canvas", L"Color", L"Performance", L"Input", L"Files" };
    renderer.DrawText(x, y, titles[std::clamp(m_settingsCategory, 0, 5)], Color(242, 246, 248), 3);
    y += 40;
    if (m_settingsCategory == 2) {
        renderer.DrawText(x, y, L"Current RGB profile", Color(156, 166, 174), 2);
        renderer.DrawText(x + 210, y, ProfileLabel(m_selectedRgbProfile), Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Current CMYK profile", Color(156, 166, 174), 2);
        renderer.DrawText(x + 210, y, ProfileLabel(m_selectedCmykProfile), Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"HDR advanced color profile unavailable in MVP", Color(238, 190, 90), 2);
    } else if (m_settingsCategory == 3) {
        renderer.DrawText(x, y, L"Internal raster pipeline", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"RGBA10 tiles, BGRA8 presentation", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Workspace present mode", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Full-frame stable MVP", Color(230, 236, 240), 2);
    } else if (m_settingsCategory == 1) {
        renderer.DrawText(x, y, L"Default canvas", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"1600 x 900, 350 dpi, RGBA10", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"New image form", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Program Manager owns creation flow", Color(230, 236, 240), 2);
    } else if (m_settingsCategory == 4) {
        renderer.DrawText(x, y, L"Pointer pipeline", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Mouse / pen normalized by adapter", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Text editor", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Planned next round", Color(238, 190, 90), 2);
    } else if (m_settingsCategory == 5) {
        renderer.DrawText(x, y, L"Project format", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L".cloverpic chunked container", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Recent projects", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Adapter-backed CloverPic store", Color(230, 236, 240), 2);
    } else {
        renderer.DrawText(x, y, L"CloverPic keeps platform details in adapter services.", Color(210, 216, 220), 2);
        y += 34;
        renderer.DrawText(x, y, L"Core owns UI scene, project schema, raster pipeline, and commands.", Color(210, 216, 220), 2);
    }
    renderer.DrawText(x, panelBottom - 88, m_settingsStatus, Color(148, 158, 166), 2);
}

void ProjectManagerRuntime::RenderButton(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    const Color fill = disabled ? Color(42, 45, 48) : (hovered ? Color(78, 86, 96) : Color(56, 62, 70));
    renderer.FillRect(node.bounds, fill);
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      disabled ? Color(64, 68, 72, 110) : Color(118, 126, 134, 130));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.bottom - 1, node.bounds.right, node.bounds.bottom),
                      Color(16, 18, 20, 170));
    renderer.StrokeRect(node.bounds, hovered ? Color(0, 120, 215) : Color(112, 122, 132), 1);
    const Color text = disabled ? Color(118, 124, 130) : Color(242, 245, 247);
    if (node.iconId != Presentation::IconId::None && node.iconPlacement == CoreUI::IconPlacement::Center) {
        const int side = std::min(node.bounds.Width(), node.bounds.Height()) - 8;
        const int left = node.bounds.left + (node.bounds.Width() - side) / 2;
        const int top = node.bounds.top + (node.bounds.Height() - side) / 2;
        if (!Presentation::IconPainter::Draw(renderer, node.iconId, Rect(left, top, left + side, top + side), text, 1)) {
            renderer.DrawText(node.bounds.left + 8, node.bounds.top + 9, node.label, text, 2);
        }
        return;
    }
    if (node.iconId != Presentation::IconId::None &&
        Presentation::IconPainter::Draw(renderer, node.iconId, Rect(node.bounds.left + 7, node.bounds.top + 6, node.bounds.left + 23, node.bounds.top + 22), text, 1)) {
        renderer.DrawText(node.bounds.left + 30, node.bounds.top + 9, node.label, text, 2);
    } else {
        renderer.DrawText(node.bounds.left + 8, node.bounds.top + 9, node.label, text, 2);
    }
}

void ProjectManagerRuntime::RenderHomeAction(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    const bool disabled = (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, hovered && !disabled ? Color(48, 48, 48) : Color(24, 24, 24));
    const Color accent = disabled ? Color(78, 82, 86)
                                  : (node.command == static_cast<uint32_t>(AppCommand::Quit) ? Color(210, 52, 56) : Color(0, 120, 215));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 5, node.bounds.bottom), accent);
    renderer.DrawText(node.bounds.left + 16, node.bounds.top + 13, node.label,
                      disabled ? Color(120, 126, 132) : Color(238, 238, 238), 2);
}

void ProjectManagerRuntime::RenderHomeRecent(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool hovered = node.id == m_scene.GetHover();
    renderer.FillRect(node.bounds, hovered ? Color(52, 52, 52) : Color(24, 24, 24));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top + 6, node.bounds.left + 30, node.bounds.top + 36), Color(0, 120, 215));
    renderer.DrawText(node.bounds.left + 42, node.bounds.top + 7, node.label, Color(238, 238, 238), 2);
}

void ProjectManagerRuntime::RenderHomeSearch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    const bool focused = node.id == m_scene.GetFocus();
    renderer.FillRect(node.bounds, Color(239, 239, 239));
    renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : Color(239, 239, 239), focused ? 3 : 1);
    const bool placeholder = m_searchQuery.empty();
    renderer.DrawText(node.bounds.left + 14, node.bounds.top + 14, placeholder ? L"Search recent projects" : m_searchQuery,
                      placeholder ? Color(112, 112, 112) : Color(32, 32, 32), 2);
}

void ProjectManagerRuntime::RenderHomeTile(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node) {
    Color fill = ColorFromPackedRgba(node.userData);
    if (node.id == m_scene.GetHover()) fill = Color::Interpolate(fill, Color(255, 255, 255), 0.12f);
    renderer.FillRect(node.bounds, fill);
    renderer.StrokeRect(node.bounds, Color(18, 18, 18), 2);
    if (!node.label.empty()) renderer.DrawText(node.bounds.left + 10, node.bounds.bottom - 24, node.label, Color(245, 248, 250), 2);
}

void ProjectManagerRuntime::StartNewCanvas(uint32_t width, uint32_t height) {
    m_launchRequest.kind = WorkspaceLaunchKind::NewCanvas;
    m_launchRequest.width = width;
    m_launchRequest.height = height;
    m_launchRequest.dpi = static_cast<float>(m_formDpi);
    m_launchRequest.transparent = m_formTransparent;
    m_launchRequest.initialLayerType = m_formTransparent ? LayerType::Transparent : LayerType::Color;
    m_launchRequest.backgroundColor = m_formBackground;
    if (m_selectedRgbProfile < m_profiles.size()) {
        m_launchRequest.rgbProfilePath = m_profiles[m_selectedRgbProfile].path;
        if (auto* provider = CoreServices::GetColorProfileProvider()) {
            provider->ReadProfileBytes(m_launchRequest.rgbProfilePath, m_launchRequest.rgbProfileBytes);
        }
    }
    if (m_selectedCmykProfile < m_profiles.size()) {
        m_launchRequest.cmykProfilePath = m_profiles[m_selectedCmykProfile].path;
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
    m_profiles.clear();
    if (auto* provider = CoreServices::GetColorProfileProvider()) {
        m_profiles = provider->EnumerateColorProfiles();
    }
    if (m_profiles.empty()) {
        ColorProfileInfo fallback;
        fallback.id = L"srgb";
        fallback.displayName = L"sRGB fallback";
        fallback.isDefault = true;
        m_profiles.push_back(fallback);
    }
    m_selectedRgbProfile = 0;
    m_selectedCmykProfile = 0;
    for (size_t i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].isDefault || m_profiles[i].isCurrentDisplayProfile) {
            m_selectedRgbProfile = i;
            break;
        }
    }
}

void ProjectManagerRuntime::SaveSettingsDraft(bool persist) {
    m_settingsStatus = persist ? L"Settings saved." : L"Settings applied for this session.";
    if (!persist) return;
    if (auto* store = CoreServices::GetAppSettingsStore()) {
        std::ostringstream stream(std::ios::binary);
        const String rgb = m_selectedRgbProfile < m_profiles.size() ? m_profiles[m_selectedRgbProfile].path : L"";
        const String cmyk = m_selectedCmykProfile < m_profiles.size() ? m_profiles[m_selectedCmykProfile].path : L"";
        auto writeString = [&](const String& value) {
            uint32_t len = static_cast<uint32_t>(value.size());
            stream.write(reinterpret_cast<const char*>(&len), sizeof(len));
            for (wchar_t ch : value) {
                uint32_t cp = static_cast<uint32_t>(ch);
                stream.write(reinterpret_cast<const char*>(&cp), sizeof(cp));
            }
        };
        writeString(rgb);
        writeString(cmyk);
        const std::string data = stream.str();
        store->SaveSettingsBytes(std::vector<uint8_t>(data.begin(), data.end()));
    }
}

String ProjectManagerRuntime::ProfileLabel(size_t index) const {
    if (index >= m_profiles.size()) return L"sRGB fallback";
    const auto& profile = m_profiles[index];
    if (!profile.displayName.empty()) return profile.displayName;
    if (!profile.path.empty()) {
        const size_t slash = profile.path.find_last_of(L"\\/");
        return slash == String::npos ? profile.path : profile.path.substr(slash + 1);
    }
    return L"sRGB fallback";
}

String ProjectManagerRuntime::ActiveFieldPayload(ActiveField field) const {
    switch (field) {
        case ActiveField::Width: return L"field:width";
        case ActiveField::Height: return L"field:height";
        case ActiveField::Dpi: return L"field:dpi";
        case ActiveField::Search: return L"field:search";
        case ActiveField::None: break;
    }
    return L"";
}

void ProjectManagerRuntime::OnShowNewCanvasModal() {
    m_page = Page::NewImage;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnShowSettingsModal() {
    m_page = Page::Settings;
    m_settingsOpenedFromWorkspace = false;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnCreateCanvasFromForm() {
    StartNewCanvas(std::clamp<uint32_t>(m_formWidth, 16, 65535),
                   std::clamp<uint32_t>(m_formHeight, 16, 65535));
    m_page = Page::Start;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSwapCanvasOrientation() {
    std::swap(m_formWidth, m_formHeight);
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnToggleCanvasTransparency() {
    m_formTransparent = !m_formTransparent;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSelectRgbProfile(size_t index) {
    if (index < m_profiles.size()) m_selectedRgbProfile = index;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSelectCmykProfile(size_t index) {
    if (index < m_profiles.size()) m_selectedCmykProfile = index;
    BuildScene();
    MarkDirty(FullRect());
}

void ProjectManagerRuntime::OnSetSettingsCategory(size_t index) {
    m_settingsCategory = static_cast<int>(std::clamp<size_t>(index, 0, 5));
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
    m_formWidth = width;
    m_formHeight = height;
    StartNewCanvas(width, height);
}

void ProjectManagerRuntime::OnCloseModal() {
    if (m_page == Page::Settings && m_settingsOpenedFromWorkspace) {
        m_settingsOpenedFromWorkspace = false;
        m_wantsQuit = true;
        return;
    }
    m_page = Page::Start;
    m_activeField = ActiveField::None;
    m_settingsOpenedFromWorkspace = false;
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
