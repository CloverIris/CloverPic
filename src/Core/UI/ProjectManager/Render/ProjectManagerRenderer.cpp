#include "Core/UI/ProjectManager/Render/ProjectManagerRenderer.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/Presentation/IconPainter.h"
#include "Core/UI/ProjectManager/Layout/ProjectManagerLayout.h"
#include <algorithm>
#include <cmath>

namespace CloverPic::Core {

namespace {

void RenderButton(Presentation::SoftRenderer& renderer,
                  const CoreUI::UiNode& node,
                  const CoreUI::UiScene& scene) {
    const bool hovered = node.id == scene.GetHover();
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

void RenderHomeAction(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const CoreUI::UiScene& scene) {
    const bool hovered = node.id == scene.GetHover();
    const bool disabled = (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, hovered && !disabled ? Color(48, 48, 48) : Color(24, 24, 24));
    const Color accent = disabled ? Color(78, 82, 86)
                                  : (node.command == static_cast<uint32_t>(AppCommand::Quit) ? Color(210, 52, 56) : Color(0, 120, 215));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 5, node.bounds.bottom), accent);
    renderer.DrawText(node.bounds.left + 16, node.bounds.top + 13, node.label,
                      disabled ? Color(120, 126, 132) : Color(238, 238, 238), 2);
}

void RenderHomeRecent(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const CoreUI::UiScene& scene) {
    const bool hovered = node.id == scene.GetHover();
    renderer.FillRect(node.bounds, hovered ? Color(52, 52, 52) : Color(24, 24, 24));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top + 6, node.bounds.left + 30, node.bounds.top + 36), Color(0, 120, 215));
    renderer.DrawText(node.bounds.left + 42, node.bounds.top + 7, node.label, Color(238, 238, 238), 2);
}

void RenderHomeSearch(Presentation::SoftRenderer& renderer,
                      const CoreUI::UiNode& node,
                      const CoreUI::UiScene& scene,
                      const ProjectManagerUiState& uiState) {
    const bool focused = node.id == scene.GetFocus();
    renderer.FillRect(node.bounds, Color(239, 239, 239));
    renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : Color(239, 239, 239), focused ? 3 : 1);
    const bool placeholder = uiState.searchQuery.empty();
    renderer.DrawText(node.bounds.left + 14, node.bounds.top + 14,
                      placeholder ? L"Search recent projects" : uiState.searchQuery,
                      placeholder ? Color(112, 112, 112) : Color(32, 32, 32), 2);
}

void RenderHomeTile(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const CoreUI::UiScene& scene) {
    Color fill = ColorFromPackedRgba(node.userData);
    if (node.id == scene.GetHover()) fill = Color::Interpolate(fill, Color(255, 255, 255), 0.12f);
    renderer.FillRect(node.bounds, fill);
    renderer.StrokeRect(node.bounds, Color(18, 18, 18), 2);
    if (!node.label.empty()) renderer.DrawText(node.bounds.left + 10, node.bounds.bottom - 24, node.label, Color(245, 248, 250), 2);
}

void RenderHome(Presentation::SoftRenderer& renderer,
                const Size& viewport,
                const ProjectManagerUiState& uiState,
                const CoreUI::UiScene& scene) {
    renderer.Clear(Color(17, 17, 17));
    renderer.FillRect(Rect(0, 0, viewport.width, 6), Color(0, 120, 215));
    renderer.FillRect(Rect(0, 6, std::max(280, viewport.width / 3), viewport.height), Color(24, 24, 24));
    renderer.FillRect(Rect(std::max(280, viewport.width / 3), 6, viewport.width, viewport.height), Color(18, 18, 18));

    const int margin = std::max(24, viewport.width / 36);
    const int leftW = std::clamp(viewport.width / 4, 260, 360);
    const int tileLeft = margin + leftW + std::max(32, viewport.width / 24);
    renderer.FillCircle(margin + 18, margin + 36, 18, Color(245, 245, 245));
    renderer.DrawText(margin + 48, margin + 27, L"CLOVERPIC", Color(242, 242, 242), 2);
    renderer.DrawText(margin, margin + 82, L"RECENT PROJECTS", Color(150, 150, 150), 2);
    renderer.DrawText(tileLeft, margin + 34, L"START", Color(235, 235, 235), 2);
    renderer.DrawText(tileLeft + 260, margin + 34, L"CLOVER CREATION", Color(155, 155, 155), 2);

    for (const auto& node : scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) RenderHomeAction(renderer, node, scene);
        else if (node.type == CoreUI::UiNodeType::RecentItem) RenderHomeRecent(renderer, node, scene);
        else if (node.type == CoreUI::UiNodeType::SearchBox) RenderHomeSearch(renderer, node, scene, uiState);
        else if (node.type == CoreUI::UiNodeType::Tile) RenderHomeTile(renderer, node, scene);
        else if (node.type == CoreUI::UiNodeType::Text) renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(118, 118, 118), 2);
        else if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node, scene);
    }
}

void RenderNewImage(Presentation::SoftRenderer& renderer,
                    const Size& viewport,
                    const ProjectManagerUiState& uiState,
                    const CoreUI::UiScene& scene) {
    const ProjectManagerLayout layout = ProjectManagerLayoutComputer::Compute(viewport, ProjectManagerPage::NewImage);
    renderer.Clear(Color(18, 20, 22));
    renderer.FillRect(Rect(0, 0, viewport.width, 6), Color(0, 120, 215));
    renderer.FillVerticalGradient(Rect(0, 6, viewport.width, viewport.height), Color(32, 35, 38), Color(18, 20, 22));
    renderer.FillRect(Rect(layout.left, layout.top, layout.left + layout.navWidth, layout.contentBottom), Color(24, 26, 28));
    renderer.StrokeRect(Rect(layout.left, layout.top, layout.left + layout.navWidth, layout.contentBottom), Color(68, 74, 80), 1);
    renderer.FillCircle(layout.left + 28, layout.top + 30, 16, Color(245, 248, 250));
    renderer.DrawText(layout.left + 56, layout.top + 20, L"CloverPic", Color(238, 242, 245), 2);
    renderer.DrawText(layout.contentLeft, layout.top + 18, L"Create New Image", Color(244, 248, 250), 3);
    renderer.DrawText(layout.contentLeft, layout.top + 50, L"10bit RGBA document, profile-aware project container", Color(150, 160, 168), 1);

    const int previewLeft = layout.contentRight - 190;
    const int previewTop = layout.top + 88;
    renderer.DrawCheckerboard(Rect(previewLeft, previewTop, layout.contentRight - 22, previewTop + 120), 10,
                              Color(68, 72, 76), Color(54, 58, 62));
    const float aspect = uiState.formHeight == 0 ? 1.0f : static_cast<float>(uiState.formWidth) / uiState.formHeight;
    int paperW = 130;
    int paperH = static_cast<int>(paperW / std::max(0.2f, aspect));
    if (paperH > 96) {
        paperH = 96;
        paperW = static_cast<int>(paperH * aspect);
    }
    const int paperL = previewLeft + ((layout.contentRight - 22 - previewLeft) - paperW) / 2;
    const int paperT = previewTop + (120 - paperH) / 2;
    renderer.FillRect(Rect(paperL, paperT, paperL + paperW, paperT + paperH),
                      uiState.formTransparent ? Color(250, 250, 250, 190) : uiState.formBackground);
    renderer.StrokeRect(Rect(paperL, paperT, paperL + paperW, paperT + paperH), Color(0, 120, 215), 2);

    for (const auto& node : scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) RenderHomeAction(renderer, node, scene);
        else if (node.type == CoreUI::UiNodeType::Button) RenderButton(renderer, node, scene);
        else if (node.type == CoreUI::UiNodeType::SearchBox) {
            const bool focused = node.id == scene.GetFocus();
            renderer.FillRect(node.bounds, Color(55, 58, 62));
            renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : Color(90, 96, 102), focused ? 2 : 1);
            renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1), Color(108, 114, 120, 170));
            renderer.DrawText(node.bounds.left + 10, node.bounds.top + 9, node.label, Color(238, 242, 245), 2);
        } else if (node.type == CoreUI::UiNodeType::Text) {
            renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(194, 202, 208), 2);
        }
    }
}

void RenderSettings(Presentation::SoftRenderer& renderer,
                    const Size& viewport,
                    const ProjectManagerUiState& uiState,
                    const CoreUI::UiScene& scene) {
    renderer.Clear(Color(24, 27, 30));
    renderer.FillVerticalGradient(Rect(0, 0, viewport.width, viewport.height), Color(39, 42, 46), Color(20, 22, 25));

    const int navW = 220;
    const int detailLeft = navW;
    renderer.FillRect(Rect(0, 0, viewport.width, viewport.height), Color(34, 37, 40));
    renderer.FillRect(Rect(0, 0, viewport.width, 6), Color(0, 120, 215));
    renderer.FillVerticalGradient(Rect(0, 6, viewport.width, 58), Color(46, 50, 54), Color(30, 33, 36));
    renderer.FillRect(Rect(0, 0, detailLeft, viewport.height), Color(25, 27, 29));
    renderer.FillRect(Rect(detailLeft, 58, detailLeft + 1, viewport.height - 58), Color(70, 76, 82));
    renderer.FillRect(Rect(0, viewport.height - 58, viewport.width, viewport.height - 57), Color(70, 76, 82));
    renderer.DrawText(20, 22, L"Settings", Color(244, 247, 249), 3);

    for (const auto& node : scene.Nodes()) {
        if (node.type == CoreUI::UiNodeType::ActionItem) {
            const bool active = static_cast<int>(node.userData) == uiState.settingsCategory;
            renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (node.id == scene.GetHover() ? Color(48, 52, 56) : Color(25, 27, 29)));
            renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.left + 4, node.bounds.bottom),
                              active ? Color(126, 201, 255) : Color(56, 62, 68));
            renderer.DrawText(node.bounds.left + 14, node.bounds.top + 12, node.label,
                              active ? Color(255, 255, 255) : Color(210, 216, 220), 2);
        } else if (node.type == CoreUI::UiNodeType::Button) {
            RenderButton(renderer, node, scene);
        }
    }

    const int x = detailLeft + 28;
    int y = 82;
    const wchar_t* titles[] = { L"General", L"Canvas", L"Color", L"Performance", L"Input", L"Files" };
    renderer.DrawText(x, y, titles[std::clamp(uiState.settingsCategory, 0, 5)], Color(242, 246, 248), 3);
    y += 40;
    if (uiState.settingsCategory == 2) {
        renderer.DrawText(x, y, L"Current RGB profile", Color(156, 166, 174), 2);
        renderer.DrawText(x + 210, y,
                          uiState.selectedRgbProfile < uiState.profiles.size() ? uiState.profiles[uiState.selectedRgbProfile].displayName : L"sRGB fallback",
                          Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Current CMYK profile", Color(156, 166, 174), 2);
        renderer.DrawText(x + 210, y,
                          uiState.selectedCmykProfile < uiState.profiles.size() ? uiState.profiles[uiState.selectedCmykProfile].displayName : L"sRGB fallback",
                          Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"HDR advanced color profile unavailable in MVP", Color(238, 190, 90), 2);
    } else if (uiState.settingsCategory == 3) {
        renderer.DrawText(x, y, L"Internal raster pipeline", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"RGBA10 tiles, BGRA8 presentation", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Workspace present mode", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Full-frame stable MVP", Color(230, 236, 240), 2);
    } else if (uiState.settingsCategory == 1) {
        renderer.DrawText(x, y, L"Default canvas", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"1600 x 900, 350 dpi, RGBA10", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"New image form", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Program Manager owns creation flow", Color(230, 236, 240), 2);
    } else if (uiState.settingsCategory == 4) {
        renderer.DrawText(x, y, L"Pointer pipeline", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Mouse / pen normalized by adapter", Color(230, 236, 240), 2);
        y += 34;
        renderer.DrawText(x, y, L"Text editor", Color(156, 166, 174), 2);
        renderer.DrawText(x + 240, y, L"Planned next round", Color(238, 190, 90), 2);
    } else if (uiState.settingsCategory == 5) {
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
    renderer.DrawText(x, viewport.height - 88, uiState.settingsStatus, Color(148, 158, 166), 2);
}

} // namespace

void ProjectManagerRenderer::Render(Presentation::SoftRenderer& renderer,
                                    const Size& viewport,
                                    const ProjectManagerUiState& uiState,
                                    const CoreUI::UiScene& scene) {
    if (uiState.page == ProjectManagerPage::NewImage) {
        RenderNewImage(renderer, viewport, uiState, scene);
    } else if (uiState.page == ProjectManagerPage::Settings) {
        RenderSettings(renderer, viewport, uiState, scene);
    } else {
        RenderHome(renderer, viewport, uiState, scene);
    }
}

} // namespace CloverPic::Core
