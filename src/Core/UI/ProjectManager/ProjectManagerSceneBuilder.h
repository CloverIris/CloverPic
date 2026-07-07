#pragma once

#include "Core/Document/EditorSession.h"
#include "Core/UI/Scene/UiScene.h"

namespace CloverPic::Core {

class ProjectManagerSceneBuilder {
public:
    static void BuildHomeScene(const Size& viewport,
                               const EditorSession& session,
                               const String& searchQuery,
                               CoreUI::UiScene& scene);
};

} // namespace CloverPic::Core
