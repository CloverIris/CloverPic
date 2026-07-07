#include "Core/UI/Workspace/Modal/WorkspaceModalRenderer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace CloverPic::Core {

namespace {

String ModalTitle(ModalKind kind) {
    switch (kind) {
        case ModalKind::Filters: return L"FILTERS";
        case ModalKind::TextLayer: return L"TEXT LAYER";
        case ModalKind::NewCanvas: return L"NEW CANVAS";
        case ModalKind::CanvasSize: return L"CANVAS SIZE";
        case ModalKind::ConfirmDeleteLayer: return L"CONFIRM DELETE";
        case ModalKind::None: break;
    }
    return L"WORKSPACE";
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

void RenderCanvasSizePreview(Presentation::SoftRenderer& renderer,
                             const Rect& rect,
                             WorkspaceEditorFacade& editor,
                             const WorkspaceCanvasSizeState& canvasSizeState) {
    renderer.FillRect(rect, Color(31, 34, 38));
    renderer.FillRect(Rect(rect.left, rect.top, rect.right, rect.top + 28), Color(36, 40, 45));
    renderer.StrokeRect(rect, Color(86, 92, 98), 1);
    renderer.DrawText(rect.left + 10, rect.top + 8, L"Resize Preview", Color(232, 238, 242), 2);

    const auto project = editor.GetProject();
    if (!project || canvasSizeState.width == 0 || canvasSizeState.height == 0) {
        renderer.DrawText(rect.left + 10, rect.top + 42, L"Enter a target width and height.", Color(170, 176, 182), 2);
        return;
    }

    const auto& canvas = project->GetCanvas();
    const uint32_t currentWidth = std::max<uint32_t>(1, canvas.widthPx);
    const uint32_t currentHeight = std::max<uint32_t>(1, canvas.heightPx);
    const uint32_t targetWidth = std::max<uint32_t>(1, canvasSizeState.width);
    const uint32_t targetHeight = std::max<uint32_t>(1, canvasSizeState.height);
    const uint32_t anchorIndex = static_cast<uint32_t>(canvasSizeState.anchor);
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

} // namespace

void WorkspaceModalRenderer::Render(Presentation::SoftRenderer& renderer,
                                    const Rect& fullRect,
                                    const Size& viewport,
                                    const WorkspaceNodeRenderContext& nodeContext,
                                    const WorkspaceCanvasSizeState& canvasSizeState) {
    renderer.FillRect(fullRect, Color(0, 0, 0, 112));

    const int modalW = nodeContext.modalKind == ModalKind::CanvasSize ? 660 : 460;
    const int modalH = nodeContext.modalKind == ModalKind::CanvasSize ? 392 : 300;
    const int left = std::max(20, viewport.width / 2 - modalW / 2);
    const int top = std::max(20, viewport.height / 2 - modalH / 2);
    const Rect modalRect(left, top, left + modalW, top + modalH);

    renderer.FillRect(modalRect, Color(40, 45, 51));
    renderer.StrokeRect(modalRect, Color(120, 132, 142), 1);
    renderer.DrawText(left + 30, top + 28, ModalTitle(nodeContext.modalKind), Color(242, 246, 248), 3);

    if (nodeContext.modalKind == ModalKind::CanvasSize) {
        RenderCanvasSizePreview(renderer, Rect(left + 392, top + 82, left + 632, top + 298), nodeContext.editor, canvasSizeState);
    }

    for (const auto& node : nodeContext.scene.Nodes()) {
        if (node.zOrder >= 100) {
            WorkspaceNodeRenderer::RenderNode(renderer, node, nodeContext);
        }
    }
}

} // namespace CloverPic::Core
