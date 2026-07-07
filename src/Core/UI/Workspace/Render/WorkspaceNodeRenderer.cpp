#include "Core/UI/Workspace/Render/WorkspaceNodeRenderer.h"
#include "Core/App/AppCommand.h"
#include "Core/App/AppCommandPayload.h"
#include "Core/Layers/BlendMode.h"
#include "Core/Layers/Layer.h"
#include "Core/Presentation/IconPainter.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace CloverPic::Core {

namespace {

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

String LeftOfSeparator(const String& value) {
    const size_t pos = value.find(L'|');
    return pos == String::npos ? value : value.substr(0, pos);
}

String RightOfSeparator(const String& value) {
    const size_t pos = value.find(L'|');
    return pos == String::npos ? L"" : value.substr(pos + 1);
}

String BlendName(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal: return L"NORMAL";
        case BlendMode::Multiply: return L"MULTIPLY";
        case BlendMode::Screen: return L"SCREEN";
        case BlendMode::Overlay: return L"OVERLAY";
        default: return L"NORMAL";
    }
}

int TextWidthApprox(const String& text, int scale) {
    return static_cast<int>(text.size()) * 7 * std::max(1, scale);
}

String EllipsizeLabel(const String& label, int maxWidth, int scale) {
    if (label.empty() || TextWidthApprox(label, scale) <= maxWidth) {
        return label;
    }
    const int charWidth = 7 * std::max(1, scale);
    const int maxChars = std::max(1, maxWidth / charWidth);
    if (maxChars <= 1) {
        return L"";
    }
    if (maxChars <= 3) {
        return label.substr(0, static_cast<size_t>(maxChars));
    }
    return label.substr(0, static_cast<size_t>(maxChars - 1)) + L"...";
}

struct ButtonContentLayout {
    Rect iconRect;
    Point textPos;
    String label;
    bool drawIcon = false;
    bool drawText = false;
    int textScale = 2;
};

ButtonContentLayout ComputeButtonContentLayout(const CoreUI::UiNode& node, bool toolbarItem) {
    ButtonContentLayout layout;
    const bool hasIcon = node.iconId != Presentation::IconId::None;
    const bool centerIcon = node.iconPlacement == CoreUI::IconPlacement::Center || node.bounds.Width() <= 36;
    if (toolbarItem) {
        layout.textScale = 1;
        layout.drawIcon = hasIcon;
        layout.drawText = !node.label.empty();
        const int iconSide = std::min(30, std::max(18, node.bounds.Width() - 20));
        const int iconLeft = node.bounds.left + (node.bounds.Width() - iconSide) / 2;
        const int labelH = layout.drawText ? 15 : 0;
        const int contentTop = node.bounds.top + std::max(4, (node.bounds.Height() - iconSide - labelH - 3) / 2);
        layout.iconRect = Rect(iconLeft, contentTop, iconLeft + iconSide, contentTop + iconSide);
        layout.label = EllipsizeLabel(node.label, node.bounds.Width() - 6, layout.textScale);
        const int textW = TextWidthApprox(layout.label, layout.textScale);
        layout.textPos = Point(node.bounds.left + std::max(2, (node.bounds.Width() - textW) / 2),
                               node.bounds.bottom - 16);
        return layout;
    }

    layout.textScale = node.bounds.Height() <= 24 ? 1 : 2;
    if (hasIcon && centerIcon) {
        layout.drawIcon = true;
        layout.drawText = false;
        const int side = std::max(8, std::min(node.bounds.Width(), node.bounds.Height()) - 8);
        layout.iconRect = Rect(node.bounds.left + (node.bounds.Width() - side) / 2,
                               node.bounds.top + (node.bounds.Height() - side) / 2,
                               node.bounds.left + (node.bounds.Width() + side) / 2,
                               node.bounds.top + (node.bounds.Height() + side) / 2);
        return layout;
    }

    const int padding = 8;
    const int iconSide = hasIcon ? std::min(18, std::max(12, node.bounds.Height() - 10)) : 0;
    const int gap = hasIcon && !node.label.empty() ? 6 : 0;
    const int maxTextWidth = std::max(0, node.bounds.Width() - padding * 2 - iconSide - gap - (node.type == CoreUI::UiNodeType::ComboBox ? 14 : 0));
    layout.label = EllipsizeLabel(node.label, maxTextWidth, layout.textScale);
    const int textW = TextWidthApprox(layout.label, layout.textScale);
    const int groupW = iconSide + gap + textW;
    const int groupLeft = node.bounds.left + std::max(padding, (node.bounds.Width() - groupW) / 2);
    const int iconTop = node.bounds.top + (node.bounds.Height() - iconSide) / 2;
    layout.drawIcon = hasIcon;
    layout.drawText = !layout.label.empty();
    if (hasIcon) {
        layout.iconRect = Rect(groupLeft, iconTop, groupLeft + iconSide, iconTop + iconSide);
    }
    const int textX = groupLeft + iconSide + gap;
    const int textH = 8 * layout.textScale;
    layout.textPos = Point(textX, node.bounds.top + (node.bounds.Height() - textH) / 2);
    return layout;
}

uint16_t SliderValueForNode(const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    switch (node.bindingKey) {
        case CoreUI::UiBindingKey::BrushSize:
            return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(context.editor.GetBrushSize()), 1, 100));
        case CoreUI::UiBindingKey::BrushOpacity:
            return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(context.editor.GetBrushOpacity() * 100), 0, 100));
        case CoreUI::UiBindingKey::BrushFlow:
            return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(context.editor.GetBrushFlow() * 100), 0, 100));
        case CoreUI::UiBindingKey::BrushSpacing:
            return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(context.editor.GetBrushSpacing() * 100), 0, 100));
        case CoreUI::UiBindingKey::BrushWetMix:
            return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(context.editor.GetBrushWetMix() * 100), 0, 100));
        case CoreUI::UiBindingKey::LayerOpacity:
            if (auto* lm = context.editor.GetLayerManager()) {
                if (auto layer = lm->GetActiveLayer()) {
                    return static_cast<uint16_t>(std::clamp<int>(static_cast<int>(layer->GetOpacity()) * 100 / 255, 0, 100));
                }
            }
            return 0;
        default:
            return BrushParamValueFromPacked(node.userData);
    }
}

void RenderButton(Presentation::SoftRenderer& renderer,
                  const CoreUI::UiNode& node,
                  const WorkspaceNodeRenderContext& context,
                  bool active) {
    active = active || node.checked;
    const bool hovered = node.id == context.scene.GetHover();
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    if (context.modalKind == ModalKind::CanvasSize && node.payload == L"canvas-anchor" &&
        node.userData == context.activeCanvasAnchor) {
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

        const ButtonContentLayout layout = ComputeButtonContentLayout(node, true);
        if (layout.drawIcon && !Presentation::IconPainter::Draw(renderer, node.iconId, layout.iconRect, content, 2)) {
            renderer.DrawText(layout.textPos.x, layout.textPos.y, layout.label.empty() ? node.label : layout.label, content, 1);
        }
        if (layout.drawText) {
            renderer.DrawText(layout.textPos.x, layout.textPos.y, layout.label, content, layout.textScale);
        }
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
    const ButtonContentLayout layout = ComputeButtonContentLayout(node, false);
    if (layout.drawIcon) {
        if (!Presentation::IconPainter::Draw(renderer, node.iconId, layout.iconRect, content, 1) && !node.label.empty()) {
            renderer.DrawText(layout.textPos.x, layout.textPos.y, layout.label.empty() ? node.label : layout.label, content, layout.textScale);
        }
    }
    if (layout.drawText) {
        renderer.DrawText(layout.textPos.x, layout.textPos.y, layout.label, content, layout.textScale);
    }
    if (node.type == CoreUI::UiNodeType::ComboBox) {
        const int midY = node.bounds.top + node.bounds.Height() / 2;
        const int x = node.bounds.right - 14;
        renderer.DrawLine(x - 4, midY - 2, x, midY + 3, content, 1);
        renderer.DrawLine(x, midY + 3, x + 4, midY - 2, content, 1);
    }
}

void RenderMenuHeader(Presentation::SoftRenderer& renderer,
                      const CoreUI::UiNode& node,
                      const WorkspaceNodeRenderContext& context) {
    const bool hovered = node.id == context.scene.GetHover();
    const bool active = !context.uiState.openMenu.empty() && node.payload == context.uiState.openMenu;
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (hovered && !disabled ? Color(48, 52, 56) : Color(27, 29, 31)));
    renderer.DrawText(node.bounds.left + 5, node.bounds.top + 5, node.label, disabled ? Color(102, 108, 112) : Color(232, 236, 238), 2);
}

void RenderMenuItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const bool hovered = node.id == context.scene.GetHover();
    const bool disabled = node.disabled || (node.flags & Presentation::UiNodeInteractive) == 0;
    renderer.FillRect(node.bounds, hovered && !disabled ? Color(82, 154, 209) : Color(32, 34, 36));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1), Color(52, 55, 58, 90));
    const String left = LeftOfSeparator(node.label);
    const String right = node.shortcut.empty() ? RightOfSeparator(node.label) : node.shortcut;
    const Color text = disabled ? Color(126, 132, 138) : Color(224, 228, 232);
    const Rect box(node.bounds.left + 8, node.bounds.top + 8, node.bounds.left + 18, node.bounds.top + 18);
    renderer.StrokeRect(box, disabled ? Color(84, 88, 92) : Color(178, 186, 194), 1);
    if (node.checked) {
        renderer.FillRect(Rect(box.left + 2, box.top + 2, box.right - 2, box.bottom - 2), disabled ? Color(90, 96, 102) : Color(0, 120, 215));
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

void RenderLayerItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    auto* lm = context.editor.GetLayerManager();
    auto layer = lm ? lm->GetLayer(static_cast<size_t>(node.userData)) : nullptr;
    const bool active = lm && node.userData == lm->GetActiveLayerIndex();
    Color fill = active ? Color(74, 101, 126) : (node.id == context.scene.GetHover() ? Color(63, 66, 69) : Color(50, 52, 54));
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

void RenderSwatch(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const Color swatch = ColorFromPackedRgba(node.userData);
    const Color current = context.editor.GetColor();
    const bool active = swatch.r == current.r && swatch.g == current.g && swatch.b == current.b && swatch.a == current.a;
    if (swatch.a == 0) {
        renderer.DrawCheckerboard(node.bounds, 5, Color(235, 235, 235), Color(180, 180, 180));
    } else {
        renderer.FillRect(node.bounds, swatch);
    }
    renderer.StrokeRect(node.bounds, active ? Color(245, 250, 255) : Color(112, 122, 132), active ? 2 : 1);
}

void RenderSlider(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const uint16_t value = SliderValueForNode(node, context);
    const bool hovered = node.id == context.scene.GetHover() || node.id == context.scene.GetCapture();
    renderer.DrawText(node.bounds.left, node.bounds.top + 2, node.label, Color(204, 210, 214), 2);
    const Rect track(node.bounds.left + 86, node.bounds.top + 10, node.bounds.right - 46, node.bounds.top + 16);
    renderer.FillRect(track, Color(25, 27, 29));
    const int knobX = track.left + (track.Width() * std::clamp<int>(value, 0, 100)) / 100;
    renderer.FillRect(Rect(track.left, track.top, knobX, track.bottom), hovered ? Color(42, 157, 244) : Color(0, 120, 215));
    renderer.FillRect(Rect(knobX - 3, track.top - 5, knobX + 3, track.bottom + 5), Color(230, 234, 237));
    std::wstringstream valueText;
    if (node.bindingKey == CoreUI::UiBindingKey::BrushSize) valueText << std::max<uint16_t>(1, value);
    else valueText << value << L"%";
    renderer.DrawText(node.bounds.right - 38, node.bounds.top + 2, valueText.str(), Color(198, 204, 208), 2);
}

void RenderSearchBox(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const bool focused = node.id == context.scene.GetFocus();
    const bool hovered = node.id == context.scene.GetHover();
    renderer.FillRect(node.bounds, Color(29, 32, 35));
    renderer.StrokeRect(node.bounds, focused ? Color(0, 120, 215) : hovered ? Color(114, 126, 138) : Color(82, 88, 94), focused ? 2 : 1);
    const String value = node.label.empty() ? L"0" : node.label;
    renderer.DrawText(node.bounds.left + 10, node.bounds.top + 10, value, Color(236, 240, 242), 2);
}

void RenderColorField(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const Hsv hsv = RgbToHsv(context.editor.GetColor());
    renderer.DrawHsvSquare(node.bounds, hsv.h);
    renderer.StrokeRect(node.bounds, Color(26, 28, 30), 2);
    const int cx = node.bounds.left + static_cast<int>(hsv.s * std::max(1, node.bounds.Width() - 1));
    const int cy = node.bounds.top + static_cast<int>((1.0f - hsv.v) * std::max(1, node.bounds.Height() - 1));
    renderer.StrokeRect(Rect(cx - 5, cy - 5, cx + 5, cy + 5), Color(255, 255, 255), 1);
    renderer.StrokeRect(Rect(cx - 6, cy - 6, cx + 6, cy + 6), Color(30, 30, 30), 1);
}

void RenderHueStrip(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const Hsv hsv = RgbToHsv(context.editor.GetColor());
    renderer.DrawHueStrip(node.bounds);
    renderer.StrokeRect(node.bounds, Color(26, 28, 30), 1);
    const int y = node.bounds.top + static_cast<int>((hsv.h / 360.0f) * std::max(1, node.bounds.Height() - 1));
    renderer.StrokeRect(Rect(node.bounds.left - 3, y - 4, node.bounds.right + 3, y + 4), Color(235, 240, 245), 1);
}

void RenderBrushPresetItem(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const bool hovered = node.id == context.scene.GetHover();
    const uint16_t size = BrushPresetSizeFromPacked(node.userData);
    const auto tip = static_cast<Render::BrushTipType>(BrushPresetTipFromPacked(node.userData));
    const bool active = static_cast<int>(context.editor.GetBrushSize()) == size && context.editor.GetBrushTip() == tip;
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

void RenderBrushSizeChip(Presentation::SoftRenderer& renderer, const CoreUI::UiNode& node, const WorkspaceNodeRenderContext& context) {
    const uint16_t size = BrushParamValueFromPacked(node.userData);
    const bool active = std::abs(context.editor.GetBrushSize() - static_cast<float>(size)) < 0.5f;
    const bool hovered = node.id == context.scene.GetHover();
    renderer.FillRect(node.bounds, active ? Color(0, 120, 215) : (hovered ? Color(62, 66, 70) : Color(47, 50, 53)));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.top, node.bounds.right, node.bounds.top + 1),
                      active ? Color(128, 204, 255, 130) : Color(96, 102, 108, 100));
    renderer.FillRect(Rect(node.bounds.left, node.bounds.bottom - 1, node.bounds.right, node.bounds.bottom), Color(18, 20, 22, 170));
    renderer.StrokeRect(node.bounds, active ? Color(138, 202, 255) : Color(72, 76, 80), 1);
    renderer.FillCircle(node.bounds.left + node.bounds.Width() / 2, node.bounds.top + 14, std::clamp<int>(size / 12, 2, 10), Color(238, 241, 244));
    renderer.DrawText(node.bounds.left + 8, node.bounds.bottom - 16, node.label, Color(204, 210, 214), 2);
}

} // namespace

void WorkspaceNodeRenderer::RenderNode(Presentation::SoftRenderer& renderer,
                                       const CoreUI::UiNode& node,
                                       const WorkspaceNodeRenderContext& context,
                                       bool active) {
    if (node.type == CoreUI::UiNodeType::Button || node.type == CoreUI::UiNodeType::ToolbarItem || node.type == CoreUI::UiNodeType::ComboBox) RenderButton(renderer, node, context, active);
    else if (node.type == CoreUI::UiNodeType::MenuHeader) RenderMenuHeader(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::MenuItem) RenderMenuItem(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::LayerItem) RenderLayerItem(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::Swatch) RenderSwatch(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::Slider) RenderSlider(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::SearchBox) RenderSearchBox(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::ColorField) RenderColorField(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::HueStrip) RenderHueStrip(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::BrushPresetItem) RenderBrushPresetItem(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::BrushSizeChip) RenderBrushSizeChip(renderer, node, context);
    else if (node.type == CoreUI::UiNodeType::Text) renderer.DrawText(node.bounds.left, node.bounds.top, node.label, Color(210, 216, 220), 2);
    else if (node.type == CoreUI::UiNodeType::MenuSeparator) renderer.FillRect(node.bounds, Color(67, 70, 73));
}

void WorkspaceNodeRenderer::RenderPanelChrome(Presentation::SoftRenderer& renderer,
                                              const WorkspacePanelComputedLayout& panel,
                                              const WorkspaceNodeRenderContext& context) {
    const bool active = context.uiState.draggingPanel && context.uiState.draggedPanelId == panel.panelId;
    renderer.FillVerticalGradient(panel.rect, Color(48, 50, 52), Color(39, 41, 43));
    renderer.FillVerticalGradient(panel.headerRect, active ? Color(20, 78, 134) : Color(40, 43, 46),
                                 active ? Color(14, 56, 96) : Color(29, 31, 34));
    renderer.StrokeRect(panel.rect, active ? Color(120, 192, 250) : Color(74, 78, 82), active ? 2 : 1);
    renderer.FillRect(Rect(panel.rect.left + 1, panel.rect.top + 1, panel.rect.right - 1, panel.rect.top + 2), Color(95, 101, 107, 120));
    renderer.FillRect(Rect(panel.rect.left, panel.rect.top + 24, panel.rect.right, panel.rect.top + 25), Color(0, 120, 215, 120));
    renderer.DrawText(panel.rect.left + 8, panel.rect.top + 6, PanelTitle(panel.panelId), Color(220, 225, 228), 2);
}

void WorkspaceNodeRenderer::RenderPanelDecorations(Presentation::SoftRenderer& renderer,
                                                   const WorkspaceDockLayoutResult& layout,
                                                   const WorkspaceNodeRenderContext& context) {
    const Color current = context.editor.GetColor();
    for (const auto& panel : layout.panels) {
        RenderPanelChrome(renderer, panel, context);
        if (panel.panelId == WorkspacePanelId::Color) {
            renderer.FillRect(Rect(panel.contentRect.left + 2, panel.contentRect.top + 10, panel.contentRect.left + 34, panel.contentRect.top + 42), current);
            renderer.StrokeRect(Rect(panel.contentRect.left + 2, panel.contentRect.top + 10, panel.contentRect.left + 34, panel.contentRect.top + 42), Color(235, 238, 240), 2);
            renderer.FillRect(Rect(panel.contentRect.left + 12, panel.contentRect.top + 32, panel.contentRect.left + 44, panel.contentRect.top + 64), Color(255, 255, 255));
            renderer.StrokeRect(Rect(panel.contentRect.left + 12, panel.contentRect.top + 32, panel.contentRect.left + 44, panel.contentRect.top + 64), Color(30, 33, 36), 1);
            std::wstringstream rgb;
            rgb << L"R:" << static_cast<int>(current.r) << L" G:" << static_cast<int>(current.g) << L" B:" << static_cast<int>(current.b);
            renderer.DrawText(panel.contentRect.left + 2, panel.contentRect.bottom - 22, rgb.str(), Color(202, 210, 216), 2);
        } else if (panel.panelId == WorkspacePanelId::BrushPreview) {
            const Rect preview(panel.contentRect.left + 2, panel.contentRect.top, panel.contentRect.right - 2, panel.contentRect.bottom - 6);
            renderer.DrawCheckerboard(preview, 8, Color(230, 230, 230), Color(204, 204, 204));
            renderer.StrokeRect(preview, Color(78, 84, 88), 1);
            renderer.FillCircle(preview.left + 52, preview.top + 39, std::max(3, static_cast<int>(context.editor.GetBrushSize() / 8)), Color(54, 54, 54));
            for (int i = 0; i < 8; ++i) {
                const int x0 = preview.left + 120 + i * 14;
                const int y0 = preview.top + 42 + static_cast<int>(std::sin(i * 0.7f) * 10.0f);
                const int x1 = preview.left + 120 + (i + 1) * 14;
                const int y1 = preview.top + 42 + static_cast<int>(std::sin((i + 1) * 0.7f) * 10.0f);
                renderer.DrawLine(x0, y0, x1, y1, current, std::max(2, static_cast<int>(context.editor.GetBrushSize() / 16)));
            }
        } else if (panel.panelId == WorkspacePanelId::Navigator) {
            const Rect navPreview(panel.contentRect.left + 8, panel.contentRect.top, panel.contentRect.right - 8, panel.contentRect.top + 116);
            renderer.DrawCheckerboard(navPreview, 10, Color(78, 80, 84), Color(65, 67, 71));
            if (auto project = context.editor.GetProject()) {
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
    }
}

} // namespace CloverPic::Core
