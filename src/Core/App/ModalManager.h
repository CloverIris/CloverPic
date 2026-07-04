#pragma once

namespace CloverPic::Core {

enum class ModalKind {
    None,
    NewCanvas,
    Filters,
    TextLayer,
    ConfirmDeleteLayer
};

class ModalManager {
public:
    void Show(ModalKind kind) { m_current = kind; }
    void Close() { m_current = ModalKind::None; }
    bool HasModal() const { return m_current != ModalKind::None; }
    ModalKind Current() const { return m_current; }

private:
    ModalKind m_current = ModalKind::None;
};

} // namespace CloverPic::Core
