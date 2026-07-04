#pragma once

#include "Core/App/AppScreen.h"
#include "Core/App/CanvasController.h"
#include "Core/App/ModalManager.h"
#include "Core/EditorSession.h"
#include "Core/Platform/PlatformInterfaces.h"
#include "Core/UI/UiScene.h"

namespace CloverPic::Core {

class AppSceneBuilder {
public:
    static void Build(AppScreen screen,
                      ModalKind modal,
                      const Size& viewport,
                      const RgbaFrame& frame,
                      const EditorSession& session,
                      CanvasController& canvas,
                      const String& homeSearchQuery,
                      CoreUI::UiScene& scene);

private:
    static void BuildHomeScene(const Size& viewport,
                               const EditorSession& session,
                               const String& searchQuery,
                               CoreUI::UiScene& scene);
    static void BuildWorkspaceScene(const Size& viewport, CanvasController& canvas, CoreUI::UiScene& scene);
    static void BuildModalScene(ModalKind modal, const Size& viewport, const RgbaFrame& frame, CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
