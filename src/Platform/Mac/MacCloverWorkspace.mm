#import "Platform/Mac/MacCloverWorkspace.h"

#include "Core/App/WorkspaceRuntime.h"
#include "Core/UI/Workspace/Clover/WorkspaceActionRouter.h"
#include "Core/UI/Workspace/Clover/WorkspaceViewModel.h"
#include "Platform/Mac/MacCloverIme.h"
#include "Platform/Mac/MacInput.h"
#include "Platform/Mac/MacString.h"
#include "clover_platform.h"

#include <cstdio>
#include <string>

#ifndef CLOVERPIC_UI_DIR
#define CLOVERPIC_UI_DIR "src/Resource/ui"
#endif

namespace {

/// A dockable panel: the host owns the header (so it can carry a drag payload) and
/// embeds the panel's document under it.
struct PanelDef {
    std::string id;
    std::string title;
    std::string source;
    /// 0 left dock, 1 right dock, 2 floating. Order within a dock is vector order.
    int side = 0;
    float fx = 0.0f;
    float fy = 0.0f;
    float fw = 240.0f;
    float fh = 220.0f;
};

std::vector<std::string> SplitAction(const std::string& action) {
    std::vector<std::string> parts;
    std::string current;
    for (const char ch : action) {
        if (ch == ':') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

std::string PanelHeader(const PanelDef& panel) {
    return "<StackPanel x:Name=\"hdr-" + panel.id + "\" Orientation=\"Horizontal\" Height=\"20\" "
           "Padding=\"8,0\" Background=\"#2F2F2F\" Drag=\"panel-" + panel.id +
           "\" Cursor=\"move\"><TextBlock Text=\"" + panel.title +
           "\" FontSize=\"10\" Foreground=\"#9C9C9C\" VerticalAlignment=\"Center\"/></StackPanel>";
}

/// Docked panels, stacked in order inside one dock container.
std::string PanelMarkup(const std::vector<PanelDef>& panels, int side) {
    std::string out;
    for (const auto& panel : panels) {
        if (panel.side != side) {
            continue;
        }
        out += "<StackPanel x:Name=\"panel-" + panel.id + "\" Width=\"Fill\" Height=\"Fill\">";
        out += PanelHeader(panel);
        out += "<Content Source=\"" + panel.source + "\" Height=\"Fill\"/>";
        out += "</StackPanel>";
    }
    return out;
}

/// A floating panel: positioned absolutely in the overlay.
std::string FloatingMarkup(const PanelDef& panel, float viewportW, float viewportH) {
    const float w = std::clamp(panel.fw, 180.0f, std::max(200.0f, viewportW - 40.0f));
    const float h = std::clamp(panel.fh, 120.0f, std::max(140.0f, viewportH - 40.0f));
    const float x = std::clamp(panel.fx, 0.0f, std::max(0.0f, viewportW - w));
    const float y = std::clamp(panel.fy, 0.0f, std::max(0.0f, viewportH - h));
    std::string out = "<StackPanel x:Name=\"panel-" + panel.id + "\" Position=\"" +
                      std::to_string(static_cast<int>(x)) + "," +
                      std::to_string(static_cast<int>(y)) + "\" Width=\"" +
                      std::to_string(static_cast<int>(w)) + "\" Height=\"" +
                      std::to_string(static_cast<int>(h)) +
                      "\" Background=\"#232323\" BorderThickness=\"1\" BorderBrush=\"#4A4A4A\">";
    out += PanelHeader(panel);
    out += "<Content Source=\"" + panel.source + "\" Height=\"Fill\"/>";
    out += "</StackPanel>";
    return out;
}

} // namespace

using CloverPic::Core::WorkspaceSnapshot;
using CloverPic::Platform::Mac::ToUtf8;

@implementation CloverWorkspaceView {
    CloverPic::Core::WorkspaceRuntime* _runtime;
    std::unique_ptr<CloverPic::Core::WorkspaceActionRouter> _router;
    clover_surface* _surface;
    NSSize _lastSize;
    float _scale;
    NSTimer* _syncTimer;
    WorkspaceSnapshot _last;
    bool _hasLast;
    std::string _lastLayerMarkup;
    uint64_t _canvasHandle;
    bool _canvasBound;
    bool _canvasDirty;
    CloverPic::Rect _canvasRect;
    bool _canvasFocused;
    std::vector<PanelDef> _panels;
    std::string _draggingPanel;
    float _grabDX;
    float _grabDY;
    float _previewX;
    float _previewY;
    float _previewW;
    float _previewH;
    bool _insertVisible;
    int _insertSide;
    int _insertIndex;
    float _insertX;
    float _insertY;
    float _insertW;
    float _dragX;
    float _dragY;
    float _pointerX;
    float _pointerY;
    int32_t _frameWidth;
    int32_t _frameHeight;
    float _frameScale;
}

- (instancetype)initWithRuntime:(CloverPic::Core::WorkspaceRuntime*)runtime {
    self = [super initWithFrame:NSMakeRect(0, 0, 1504, 879)];
    if (self) {
        _runtime = runtime;
        _router = std::make_unique<CloverPic::Core::WorkspaceActionRouter>(*runtime);
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}

- (float)currentScale {
    NSWindow* window = self.window;
    return window ? static_cast<float>(window.backingScaleFactor) : 1.0f;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (!self.window) {
        return;
    }
    [self createSurfaceIfNeeded];
    [self.window makeFirstResponder:self];
    [self syncFromRuntime];
    _syncTimer = [NSTimer timerWithTimeInterval:1.0 / 30.0
                                         target:self
                                       selector:@selector(timerFired:)
                                       userInfo:nil
                                        repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_syncTimer forMode:NSRunLoopCommonModes];
}

- (void)timerFired:(NSTimer*)timer {
    (void)timer;
    [self syncFromRuntime];
    [self setNeedsDisplay:YES];
}

/// Places the canvas node and keeps the core's viewport in step with it.
///
/// The runtime is also resized here: the core view that used to do it is gone in
/// this mode, and without a sized frame the canvas has nowhere to render.
- (void)syncCanvas {
    if (!_surface || !_runtime) {
        return;
    }
    const float scale = [self currentScale];
    const NSRect bounds = self.bounds;
    const int32_t frameWidth = static_cast<int32_t>(std::lround(NSWidth(bounds) * scale));
    const int32_t frameHeight = static_cast<int32_t>(std::lround(NSHeight(bounds) * scale));

    clover_rect node{};
    if (clover_surface_node_bounds(_surface, "canvas", &node) != CLOVER_OK) {
        return;
    }
    CloverPic::Rect rect(static_cast<int32_t>(std::lround(node.x * scale)),
                         static_cast<int32_t>(std::lround(node.y * scale)),
                         static_cast<int32_t>(std::lround((node.x + node.w) * scale)),
                         static_cast<int32_t>(std::lround((node.y + node.h) * scale)));
    const int32_t width = rect.right - rect.left;
    const int32_t height = rect.bottom - rect.top;
    if (width < 2 || height < 2) {
        return;
    }

    const bool sizeChanged =
        _canvasRect.right - _canvasRect.left != width || _canvasRect.bottom - _canvasRect.top != height;
    _canvasRect = rect;
    // Resize reallocates and clears the whole frame, so it must only run when the
    // size or scale actually changed.
    if (_frameWidth != frameWidth || _frameHeight != frameHeight ||
        std::fabs(_frameScale - scale) > 0.001f) {
        _frameWidth = frameWidth;
        _frameHeight = frameHeight;
        _frameScale = scale;
        _runtime->Resize(static_cast<uint32_t>(frameWidth), static_cast<uint32_t>(frameHeight), scale);
        _canvasDirty = true;
    }
    // The render target is canvas-sized and starts at (0,0), so the runtime's
    // viewport must be the same rect — otherwise input maps from the frame origin
    // while rendering maps from the buffer origin, and the brush cursor drifts by
    // exactly the canvas offset.
    _runtime->SetCanvasViewport(CloverPic::Rect(0, 0, width, height));

    if (!_canvasBound) {
        _canvasHandle = clover_surface_add_direct(_surface, static_cast<uint32_t>(width),
                                                  static_cast<uint32_t>(height));
        clover_surface_bind_direct(_surface, "canvas", _canvasHandle);
        _canvasBound = true;
        _canvasDirty = true;
    } else if (sizeChanged) {
        clover_direct_resize(_surface, _canvasHandle, static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height));
        _canvasDirty = true;
    }
}

/// Copies the canvas region out of the core frame into the direct surface.
///
/// Only when the canvas actually changed: a full copy of a 1200x800 region at 2x
/// is 15 MB, which is not something to do every frame.
- (void)paintCanvasIfNeeded {
    if (!_canvasDirty || !_canvasBound || !_runtime) {
        return;
    }
    uint32_t* pixels = nullptr;
    size_t stride = 0;
    uint32_t rows = 0;

    // During a stroke only the touched region is repainted; the rest of the image
    // survives because the incremental write starts from the published frame.
    CloverPic::Rect damage;
    const bool partial = _runtime->TakeCanvasDamage(damage);
    const clover_result locked =
        partial ? clover_direct_begin_write_incremental(_surface, _canvasHandle, &pixels, &stride, &rows)
                : clover_direct_begin_write(_surface, _canvasHandle, &pixels, &stride, &rows);
    if (locked != CLOVER_OK || !pixels) {
        return;
    }

    const CloverPic::Rect* dirty = nullptr;
    CloverPic::Rect clipped;
    if (partial) {
        clipped = CloverPic::Rect(std::max(0, damage.left), std::max(0, damage.top),
                                  std::min(static_cast<int32_t>(stride), damage.right),
                                  std::min(static_cast<int32_t>(rows), damage.bottom));
        if (clipped.right <= clipped.left || clipped.bottom <= clipped.top) {
            clover_direct_commit(_surface, _canvasHandle);
            _canvasDirty = false;
            return;
        }
        dirty = &clipped;
    }

    // Zero copy: the editor renders straight into the surface's back buffer.
    const bool painted = _runtime->RenderCanvasInto(
        reinterpret_cast<uint8_t*>(pixels), static_cast<uint32_t>(stride), rows,
        static_cast<uint32_t>(stride * 4), dirty);
    if (partial && painted) {
        // Publish and repaint only the damaged part of the page. The canvas box is
        // positioned in logical points, so the damage is divided by the scale.
        const float scale = [self currentScale];
        clover_rect node{};
        clover_surface_node_bounds(_surface, "canvas", &node);
        const float x = static_cast<float>(node.x) + static_cast<float>(clipped.left) / scale;
        const float y = static_cast<float>(node.y) + static_cast<float>(clipped.top) / scale;
        clover_direct_commit_rect(_surface, _canvasHandle, x, y,
                                  static_cast<float>(clipped.right - clipped.left) / scale,
                                  static_cast<float>(clipped.bottom - clipped.top) / scale);
    } else {
        clover_direct_commit(_surface, _canvasHandle);
    }
    // If the project was not ready yet, try again next frame.
    _canvasDirty = !painted;
}

/// Node bounds for a panel wrapper, or an empty rect when it is not on screen.
- (clover_rect)boundsOfPanel:(const std::string&)id {
    clover_rect rect{};
    const std::string name = "panel-" + id;
    if (clover_surface_node_bounds(_surface, name.c_str(), &rect) != CLOVER_OK) {
        rect.x = rect.y = rect.w = rect.h = 0;
    }
    return rect;
}

/// Where a dragged panel would be inserted on `side`, given the pointer's y.
///
/// Ported from the core scene: walk the docked panels in order and stop at the
/// first whose vertical midpoint is below the pointer.
- (int)insertIndexForSide:(int)side y:(float)y {
    int index = 0;
    for (const auto& panel : _panels) {
        if (panel.side != side) {
            continue;
        }
        const clover_rect rect = [self boundsOfPanel:panel.id];
        if (rect.h > 0 && y < rect.y + rect.h * 0.5f) {
            return index;
        }
        ++index;
    }
    return index;
}

/// Rebuilds the dock containers, the floating panels and any drag feedback.
- (void)rebuildPanelViews {
    if (!_surface) {
        return;
    }
    clover_surface_set_children_xaml(_surface, "leftPanels", PanelMarkup(_panels, 0).c_str());
    clover_surface_set_children_xaml(_surface, "rightPanels", PanelMarkup(_panels, 1).c_str());

    std::string overlay;
    for (const auto& panel : _panels) {
        if (panel.side == 2) {
            overlay += FloatingMarkup(panel, static_cast<float>(_frameWidth),
                                      static_cast<float>(_frameHeight));
        }
    }
    if (_draggingPanel.empty() == false) {
        // The ghost the user is dragging, plus the bar showing where it will land.
        overlay += "<Panel Position=\"" + std::to_string(static_cast<int>(_previewX)) + "," +
                   std::to_string(static_cast<int>(_previewY)) + "\" Width=\"" +
                   std::to_string(static_cast<int>(_previewW)) + "\" Height=\"" +
                   std::to_string(static_cast<int>(_previewH)) +
                   "\" Background=\"#2B3138\" BorderThickness=\"1\" BorderBrush=\"#5B9CFF\" "
                   "CornerRadius=\"4\"/>";
        if (_insertVisible) {
            overlay += "<Panel Position=\"" + std::to_string(static_cast<int>(_insertX)) + "," +
                       std::to_string(static_cast<int>(_insertY)) + "\" Width=\"" +
                       std::to_string(static_cast<int>(_insertW)) +
                       "\" Height=\"6\" Background=\"#5B9CFF\" CornerRadius=\"3\"/>";
        }
    }
    clover_surface_set_overlay_cui(_surface, ("<Panel>" + overlay + "</Panel>").c_str());
    [self setNeedsDisplay:YES];
}

- (void)beginPanelDrag:(const std::string&)id x:(float)x y:(float)y {
    for (auto& panel : _panels) {
        if (panel.id != id) {
            continue;
        }
        const clover_rect rect = [self boundsOfPanel:id];
        _draggingPanel = id;
        _grabDX = x - static_cast<float>(rect.x);
        _grabDY = y - static_cast<float>(rect.y);
        _previewW = panel.side == 2 ? panel.fw : std::max(220.0f, static_cast<float>(rect.w));
        _previewH = panel.side == 2 ? panel.fh : std::max(120.0f, static_cast<float>(rect.h));
        _previewX = x - _grabDX;
        _previewY = y - _grabDY;
        break;
    }
}

/// Recomputes the ghost and the insertion marker from the current pointer.
- (void)updatePanelDragAtX:(float)x y:(float)y {
    if (_draggingPanel.empty()) {
        return;
    }
    _previewX = x - _grabDX;
    _previewY = y - _grabDY;
    _insertVisible = false;

    clover_rect left{};
    clover_rect right{};
    const bool hasLeft = clover_surface_node_bounds(_surface, "leftPanels", &left) == CLOVER_OK;
    const bool hasRight = clover_surface_node_bounds(_surface, "rightPanels", &right) == CLOVER_OK;

    int side = -1;
    if (hasLeft && x >= left.x && x < left.x + left.w && y >= left.y - 40 && y < left.y + left.h) {
        side = 0;
    } else if (hasRight && x >= right.x && x < right.x + right.w && y >= right.y - 40 &&
               y < right.y + right.h) {
        side = 1;
    }

    if (side >= 0) {
        _insertSide = side;
        _insertIndex = [self insertIndexForSide:side y:y];
        const clover_rect dock = side == 0 ? left : right;
        // The bar sits at the top edge of the panel at the insertion index, or at
        // the bottom of the dock when appending.
        float barY = static_cast<float>(dock.y + dock.h);
        int seen = 0;
        for (const auto& panel : _panels) {
            if (panel.side != side) {
                continue;
            }
            if (seen == _insertIndex) {
                const clover_rect rect = [self boundsOfPanel:panel.id];
                barY = static_cast<float>(rect.y) - 3.0f;
                break;
            }
            ++seen;
        }
        _insertX = static_cast<float>(dock.x + 8);
        _insertY = barY;
        _insertW = static_cast<float>(dock.w - 16);
        _insertVisible = true;
    }
    [self rebuildPanelViews];
}

- (void)commitPanelDrag {
    if (_draggingPanel.empty()) {
        return;
    }
    PanelDef* dragged = nullptr;
    for (auto& panel : _panels) {
        if (panel.id == _draggingPanel) {
            dragged = &panel;
            break;
        }
    }
    if (!dragged) {
        _draggingPanel.clear();
        return;
    }

    if (_insertVisible) {
        // Remove from its current position, then insert at the requested index.
        PanelDef moved = *dragged;
        _panels.erase(std::remove_if(_panels.begin(), _panels.end(),
                                     [&](const PanelDef& p) { return p.id == moved.id; }),
                      _panels.end());
        moved.side = _insertSide;
        int seen = 0;
        auto insertAt = _panels.end();
        for (auto it = _panels.begin(); it != _panels.end(); ++it) {
            if (it->side != _insertSide) {
                continue;
            }
            if (seen == _insertIndex) {
                insertAt = it;
                break;
            }
            ++seen;
        }
        _panels.insert(insertAt, moved);
    } else {
        // Dropped away from both docks: it becomes a floating panel.
        dragged->side = 2;
        dragged->fx = _previewX;
        dragged->fy = _previewY;
        dragged->fw = _previewW;
        dragged->fh = _previewH;
    }
    _draggingPanel.clear();
    _insertVisible = false;
    [self rebuildPanelViews];
    [self savePanelLayout];
}

- (void)savePanelLayout {
    NSString* dir = nil;
    if (const char* data = std::getenv("CLOVERPIC_DATA_DIR")) {
        dir = [NSString stringWithUTF8String:data];
    }
    if (!dir) {
        dir = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES)
            .firstObject stringByAppendingPathComponent:@"CloverPic"];
    }
    [[NSFileManager defaultManager] createDirectoryAtPath:dir
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
    std::string text;
    for (const auto& panel : _panels) {
        text += panel.id + " " + std::to_string(panel.side) + " " +
                std::to_string(static_cast<int>(panel.fx)) + " " +
                std::to_string(static_cast<int>(panel.fy)) + " " +
                std::to_string(static_cast<int>(panel.fw)) + " " +
                std::to_string(static_cast<int>(panel.fh)) + "\n";
    }
    NSString* path = [dir stringByAppendingPathComponent:@"panels.layout"];
    [[NSString stringWithUTF8String:text.c_str()] writeToFile:path
                                                  atomically:YES
                                                    encoding:NSUTF8StringEncoding
                                                       error:nil];
}

- (void)loadPanelLayout {
    NSString* dir = nil;
    if (const char* data = std::getenv("CLOVERPIC_DATA_DIR")) {
        dir = [NSString stringWithUTF8String:data];
    }
    if (!dir) {
        dir = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES)
            .firstObject stringByAppendingPathComponent:@"CloverPic"];
    }
    NSString* path = [dir stringByAppendingPathComponent:@"panels.layout"];
    NSString* text = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    if (!text) {
        return;
    }
    std::istringstream stream(text.UTF8String);
    std::string id;
    int side = 0;
    float fx = 0, fy = 0, fw = 240, fh = 220;
    std::vector<PanelDef> ordered;
    while (stream >> id >> side >> fx >> fy >> fw >> fh) {
        for (const auto& panel : _panels) {
            if (panel.id == id) {
                PanelDef copy = panel;
                copy.side = side;
                copy.fx = fx;
                copy.fy = fy;
                copy.fw = fw;
                copy.fh = fh;
                ordered.push_back(copy);
                break;
            }
        }
    }
    // Keep any panel the file did not mention.
    for (const auto& panel : _panels) {
        const bool known = std::any_of(ordered.begin(), ordered.end(),
                                       [&](const PanelDef& p) { return p.id == panel.id; });
        if (!known) {
            ordered.push_back(panel);
        }
    }
    if (!ordered.empty()) {
        _panels = ordered;
    }
}

- (void)createSurfaceIfNeeded {
    const NSRect bounds = self.bounds;
    if (NSWidth(bounds) < 2.0 || NSHeight(bounds) < 2.0) {
        return;
    }
    const float scale = [self currentScale];
    const std::string path = std::string(CLOVERPIC_UI_DIR) + "/workspace.cui";
    _surface = clover_surface_create_from_cui(static_cast<float>(NSWidth(bounds)),
                                              static_cast<float>(NSHeight(bounds)), scale,
                                              path.c_str());
    if (!_surface) {
        std::fprintf(stderr, "[clover-workspace] failed to load %s\n", path.c_str());
        return;
    }
    _scale = scale;
    _lastSize = bounds.size;
    _panels = {
        {"presets", "BRUSH: PEN", "panels/brush-presets.cui", 0, 0, 0, 240, 220},
        {"sizes", "BRUSH SIZE", "panels/brush-sizes.cui", 1, 0, 0, 240, 220},
    };
    [self loadPanelLayout];
    [self rebuildPanelViews];
    std::fprintf(stderr, "[clover-workspace] shell loaded from %s\n", path.c_str());
}

- (void)pushLabel:(const char*)name value:(const std::string&)value {
    if (_surface) {
        clover_surface_set_label(_surface, name, value.c_str());
    }
}

- (void)syncFromRuntime {
    if (!_runtime) {
        return;
    }
    const WorkspaceSnapshot snapshot = CloverPic::Core::CaptureWorkspaceSnapshot(
        _runtime->Editor(), _runtime->UiState(), _runtime->Session().GetCurrentFilePath());

    if (!_hasLast || snapshot.documentName != _last.documentName) {
        [self pushLabel:"statusDoc" value:ToUtf8(snapshot.documentName)];
    }
    if (!_hasLast || snapshot.activeTool != _last.activeTool) {
        [self pushLabel:"statusTool" value:ToUtf8(snapshot.activeTool)];
    }
    if (!_hasLast || snapshot.zoomPercent != _last.zoomPercent) {
        [self pushLabel:"statusZoom" value:"ZOOM " + std::to_string(snapshot.zoomPercent) + "%"];
    }
    if (!_hasLast || snapshot.brushSize != _last.brushSize) {
        [self pushLabel:"statusSize" value:"SIZE " + std::to_string(static_cast<int>(snapshot.brushSize + 0.5f))];
        clover_surface_set_value(_surface, "size", snapshot.brushSize);
    }
    if (!_hasLast || snapshot.brushOpacity != _last.brushOpacity) {
        clover_surface_set_value(_surface, "opacity", snapshot.brushOpacity * 100.0f);
    }
    if (!_hasLast || snapshot.blendModes != _last.blendModes) {
        std::string csv;
        for (size_t index = 0; index < snapshot.blendModes.size(); ++index) {
            if (index) {
                csv += ",";
            }
            csv += ToUtf8(snapshot.blendModes[index]);
        }
        clover_surface_set_items(_surface, "blend", csv.c_str());
    }
    if (!_hasLast || snapshot.blendIndex != _last.blendIndex) {
        clover_surface_set_selected(_surface, "blend", snapshot.blendIndex);
    }
    if (!_hasLast || snapshot.canvasWidth != _last.canvasWidth ||
        snapshot.canvasHeight != _last.canvasHeight) {
        [self pushLabel:"statusCanvas" value:std::to_string(snapshot.canvasWidth) + " " +
                                            std::to_string(snapshot.canvasHeight)];
    }

    // The layer list is the only structural part: rebuild it when it really
    // changed, not every tick.
    std::string markup;
    for (size_t index = 0; index < snapshot.layers.size(); ++index) {
        const auto& row = snapshot.layers[index];
        markup += "<Panel Height=\"28\" Width=\"Fill\" Padding=\"8,0\" ";
        markup += row.active ? "Background=\"#3B6FD4\" Command=\"layer:select:"
                             : "Background=\"#22262B\" Command=\"layer:select:";
        markup += std::to_string(index);
        markup += "\"><Panel Width=\"18\" Height=\"18\" Background=\"";
        markup += row.visible ? "#FFFFFF" : "#4A4A4A";
        markup += "\" CornerRadius=\"2\" Margin=\"0,0,8,0\"/>";
        markup += "<TextBlock Text=\"";
        markup += ToUtf8(row.name);
        markup += "\" Foreground=\"";
        markup += row.active ? "#FFFFFF" : "#C8C8C8";
        markup += "\"/></Panel>";
    }
    if (markup != _lastLayerMarkup) {
        clover_surface_set_children_xaml(_surface, "layerList",
                                         ("<Panel>" + markup + "</Panel>").c_str());
        _lastLayerMarkup = markup;
    }

    _last = snapshot;
    _hasLast = true;
}

- (void)handleAction:(const std::string&)action {
    // Any command can change the document, so the canvas is 'dirty' from here.
    _canvasDirty = true;
    if (_router && _router->Handle(action)) {
        return;
    }
    // A submenu parent keeps the menu open; cascade beside it.
    if (action.rfind("drag:", 0) == 0) {
        const auto parts = SplitAction(action);
        if (parts.size() >= 3) {
            const std::string& phase = parts[1];
            std::string payload = parts[2];
            if (payload.rfind("panel-", 0) == 0) {
                payload = payload.substr(6);
                if (phase == "move") {
                    [self updatePanelDragAtX:_pointerX y:_pointerY];
                }
                if (phase == "begin") {
                    [self beginPanelDrag:payload x:_pointerX y:_pointerY];
                    [self updatePanelDragAtX:_pointerX y:_pointerY];
                } else if (phase == "end") {
                    // Recompute from the release point: the last move may have been
                    // coalesced, and the drop must reflect where the button came up.
                    [self updatePanelDragAtX:_pointerX y:_pointerY];
                    [self commitPanelDrag];
                }
            }
        }
        return;
    }
    if (action.rfind("panelmove:", 0) == 0) {
        const int target = action == "panelmove:0" ? 0 : 1;
        for (auto& panel : _panels) {
            if (panel.side != target) {
                panel.side = target;
                [self rebuildPanelViews];
                [self savePanelLayout];
                break;
            }
        }
        return;
    }
    if (action == "layer:2") {
        clover_surface_open_submenu(_surface, "Merge Down,Flatten Layer", "merge");
        return;
    }
    std::fprintf(stderr, "[clover-workspace] unhandled action: %s\n", action.c_str());
}

/// Returns true when the point is inside the canvas, after forwarding it to the
/// editor. The UI must not also see the event, or panels would react to strokes.
- (bool)forwardToCanvas:(clover_pointer_action)kind button:(uint32_t)button event:(NSEvent*)event {
    if (!_canvasBound || !_runtime) {
        return false;
    }
    const float scale = [self currentScale];
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    const int32_t x = static_cast<int32_t>(std::lround(local.x * scale));
    const int32_t y = static_cast<int32_t>(std::lround(local.y * scale));
    if (x < _canvasRect.left || x >= _canvasRect.right || y < _canvasRect.top ||
        y >= _canvasRect.bottom) {
        return false;
    }
    CloverPic::Input::PointerEvent pointer;
    pointer.device = CloverPic::Input::PointerDeviceType::Mouse;
    switch (kind) {
        case CLOVER_POINTER_DOWN: pointer.action = CloverPic::Input::PointerAction::Down; break;
        case CLOVER_POINTER_UP: pointer.action = CloverPic::Input::PointerAction::Up; break;
        case CLOVER_POINTER_CANCEL: pointer.action = CloverPic::Input::PointerAction::Up; break;
        default: pointer.action = CloverPic::Input::PointerAction::Move; break;
    }
    // Pointer coordinates are canvas-local, matching the (0,0)-based viewport.
    pointer.position = CloverPic::Point(x - _canvasRect.left, y - _canvasRect.top);
    pointer.button = button == 2 ? CloverPic::MouseButton::Right
                                 : (button == 1 ? CloverPic::MouseButton::Left
                                                : CloverPic::MouseButton::None);
    pointer.pressure = 1.0f;
    pointer.inContact = kind == CLOVER_POINTER_DOWN || (kind == CLOVER_POINTER_MOVE && button != 0);
    pointer.modifiers = 0;
    _runtime->HandlePointer(pointer);
    _canvasDirty = true;
    return true;
}

- (void)pushPointer:(clover_pointer_action)kind button:(uint32_t)button event:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    // A panel drag belongs to the UI even while the pointer is over the canvas.
    // Forwarding there would swallow the release and the drag would never end.
    const bool dragging = !_draggingPanel.empty();
    if (!dragging && [self forwardToCanvas:kind button:button event:event]) {
        // Pressing the canvas takes keyboard focus; releasing it must not give the
        // focus back, or every shortcut after a stroke would go to the chrome.
        if (kind == CLOVER_POINTER_DOWN) {
            _canvasFocused = true;
        }
        [[NSCursor crosshairCursor] set];
        [self setNeedsDisplay:YES];
        return;
    }
    if (kind == CLOVER_POINTER_DOWN) {
        // Clicking the chrome moves focus away from the canvas.
        _canvasFocused = false;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    // Tracked here because the drop target should be where this view saw the
    // pointer, not a value that travelled through the action string.
    _pointerX = static_cast<float>(local.x);
    _pointerY = static_cast<float>(local.y);
    clover_surface_pointer(_surface, static_cast<float>(local.x), static_cast<float>(local.y), kind,
                           button, 0);
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    [self.window makeFirstResponder:self];
    [self pushPointer:CLOVER_POINTER_DOWN button:1 event:event];
}

- (void)mouseUp:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_UP button:1 event:event];
}

- (void)mouseDragged:(NSEvent*)event {
    [self pushPointer:CLOVER_POINTER_MOVE button:1 event:event];
}

/// Right-click on the layer list opens the layer menu.
///
/// Menus are not part of the page tree: the host decides what a right-click
/// means, and the framework only places and runs the popup.
- (bool)pointInsideNode:(const char*)name x:(CGFloat)x y:(CGFloat)y {
    clover_rect rect{};
    if (clover_surface_node_bounds(_surface, name, &rect) != CLOVER_OK) {
        return false;
    }
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

- (void)rightMouseDown:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];

    // A panel header offers to move its panel to the other dock. This is also the
    // keyboard/pointer alternative when dragging is impractical.
    if (!_panels.empty() &&
        ([self pointInsideNode:"leftPanels" x:local.x y:local.y] ||
         [self pointInsideNode:"rightPanels" x:local.x y:local.y])) {
        clover_surface_open_menu(_surface, (float)local.x, (float)local.y, 200.0f, 20.0f,
                                 "Move to Left Dock,Move to Right Dock", "panelmove");
        [self setNeedsDisplay:YES];
        return;
    }

    if ([self pointInsideNode:"layerList" x:local.x y:local.y]) {
        clover_surface_open_menu(_surface, (float)local.x, (float)local.y, 180.0f, 20.0f,
                                 "Duplicate,Delete,Merge Down>,Toggle Visible", "layer");
        [self setNeedsDisplay:YES];
    }
}

- (void)mouseMoved:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    switch (clover_surface_cursor(_surface, static_cast<float>(local.x),
                                 static_cast<float>(local.y))) {
        case 1: [[NSCursor pointingHandCursor] set]; break;
        case 2: [[NSCursor IBeamCursor] set]; break;
        case 3: [[NSCursor resizeLeftRightCursor] set]; break;
        case 4: [[NSCursor resizeUpDownCursor] set]; break;
        case 5: [[NSCursor openHandCursor] set]; break;
        case 6: [[NSCursor crosshairCursor] set]; break;
        default: [[NSCursor arrowCursor] set]; break;
    }
    [self pushPointer:CLOVER_POINTER_MOVE button:0 event:event];
}

- (void)scrollWheel:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    const NSPoint local = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat delta = event.scrollingDeltaY;
    if (event.hasPreciseScrollingDeltas) {
        delta /= 10.0;
    }
    if (std::abs(delta) < 0.01) {
        return;
    }
    // Over the canvas the wheel zooms and pans the document; everywhere else it
    // scrolls the UI.
    const float scale = [self currentScale];
    const int32_t x = static_cast<int32_t>(std::lround(local.x * scale));
    const int32_t y = static_cast<int32_t>(std::lround(local.y * scale));
    if (_canvasBound && x >= _canvasRect.left && x < _canvasRect.right && y >= _canvasRect.top &&
        y < _canvasRect.bottom) {
        _runtime->HandleWheel(static_cast<int>(std::lround(delta)),
                              CloverPic::Point(x - _canvasRect.left, y - _canvasRect.top));
        _canvasDirty = true;
        [self setNeedsDisplay:YES];
        return;
    }
    clover_surface_wheel(_surface, (float)local.x, (float)local.y, (float)delta, 0);
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent*)event {
    if (!_surface) {
        return;
    }
    if (_canvasFocused) {
        const CloverPic::Input::KeyEvent key =
            CloverPic::Platform::Mac::TranslateKeyEvent(event, CloverPic::Input::KeyAction::Down);
        if (key.keyCode != 0) {
            _runtime->HandleKey(key);
            _canvasDirty = true;
            [self setNeedsDisplay:YES];
            return;
        }
    }
    [self interpretKeyEvents:@[ event ]];
}

- (void)keyUp:(NSEvent*)event {
    if (!_surface || !_canvasFocused) {
        return;
    }
    const CloverPic::Input::KeyEvent key =
        CloverPic::Platform::Mac::TranslateKeyEvent(event, CloverPic::Input::KeyAction::Up);
    if (key.keyCode != 0) {
        _runtime->HandleKey(key);
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!_surface) {
        [self createSurfaceIfNeeded];
        if (!_surface) {
            return;
        }
    }
    if (!NSEqualSizes(self.bounds.size, _lastSize)) {
        _lastSize = self.bounds.size;
        clover_surface_resize(_surface, static_cast<float>(NSWidth(self.bounds)),
                              static_cast<float>(NSHeight(self.bounds)), [self currentScale]);
    }
    [self syncFromRuntime];
    [self syncCanvas];
    [self paintCanvasIfNeeded];

    clover_frame frame{};
    if (clover_surface_frame(_surface, &frame) != CLOVER_OK || !frame.pixels) {
        return;
    }
    char buffer[512];
    while (clover_surface_take_action(_surface, buffer, sizeof(buffer)) > 0) {
        [self handleAction:std::string(buffer)];
    }

    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        const_cast<uint32_t*>(frame.pixels), frame.width, frame.height, 8, frame.stride * 4, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (!context) {
        return;
    }
    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    if (!image) {
        return;
    }
    CGContextRef target = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(target);
    CGContextTranslateCTM(target, 0.0, NSHeight(self.bounds));
    CGContextScaleCTM(target, 1.0, -1.0);
    CGContextDrawImage(target, CGRectMake(0.0, 0.0, NSWidth(self.bounds), NSHeight(self.bounds)),
                       image);
    CGContextRestoreGState(target);
    CGImageRelease(image);
}

@end
