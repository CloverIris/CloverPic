#pragma once

#include <string>

namespace CloverPic::Core {

class WorkspaceRuntime;

/// Maps a framework action string onto a workspace command.
///
/// The framework reports what happened (`change:size:71`); it knows nothing about
/// brushes, layers or tools. This is the only place that translation happens, and
/// it returns false for actions it does not own so the host can log them.
class WorkspaceActionRouter {
public:
    explicit WorkspaceActionRouter(WorkspaceRuntime& runtime) : m_runtime(&runtime) {}

    /// Returns true when the action was recognised and applied.
    bool Handle(const std::string& action);

private:
    WorkspaceRuntime* m_runtime = nullptr;
};

} // namespace CloverPic::Core
