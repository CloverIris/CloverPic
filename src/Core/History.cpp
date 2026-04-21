#include "Core/History.h"
#include "Core/Layer.h"
#include "Render/TilePool.h"
#include <cstring>
#include <iostream>

namespace VividPic {

// StrokeUndoItem implementation
StrokeUndoItem::StrokeUndoItem(Layer* layer) : m_layer(layer) {}

void StrokeUndoItem::CaptureTile(uint32_t gridX, uint32_t gridY, const uint8_t* data, size_t size) {
    uint32_t key = gridX | (gridY << 16);
    if (m_capturedSet.find(key) != m_capturedSet.end()) return;

    TileSnapshot snap;
    snap.gridX = gridX;
    snap.gridY = gridY;
    snap.beforeData.resize(size);
    std::memcpy(snap.beforeData.data(), data, size);
    m_snapshots.push_back(std::move(snap));
    m_capturedSet.insert(key);
}

bool StrokeUndoItem::HasTile(uint32_t gridX, uint32_t gridY) const {
    uint32_t key = gridX | (gridY << 16);
    return m_capturedSet.find(key) != m_capturedSet.end();
}

size_t StrokeUndoItem::GetMemorySize() const {
    size_t total = 0;
    for (const auto& snap : m_snapshots) {
        total += snap.beforeData.size();
    }
    return total;
}

void StrokeUndoItem::Undo() {
    if (!m_layer || m_snapshots.empty()) return;
    std::cout << "[History] Undo stroke, tiles=" << m_snapshots.size() << std::endl;
    for (const auto& snap : m_snapshots) {
        Render::Tile* tile = m_layer->GetTile(snap.gridX, snap.gridY);
        if (!tile) continue;
        std::memcpy(tile->data, snap.beforeData.data(), snap.beforeData.size());
    }
    m_layer->MarkDirty();
}

void StrokeUndoItem::Redo() {
    // For stroke-based undo, we currently don't store after-image.
    // Redo would require replaying the stroke or storing after-image.
    // For now, Redo is no-op (same behavior as many simple painting apps).
    // TODO: implement after-image snapshot for full redo support.
}

// HistoryManager implementation
HistoryManager& HistoryManager::GetInstance() {
    static HistoryManager instance;
    return instance;
}

void HistoryManager::Push(std::unique_ptr<StrokeUndoItem> item) {
    if (!item || item->IsEmpty()) return;
    m_undoStack.push_back(std::move(item));
    m_redoStack.clear();
    if (m_undoStack.size() > m_maxSteps) {
        m_undoStack.erase(m_undoStack.begin());
    }
    std::cout << "[History] Push undo, stack=" << m_undoStack.size() << std::endl;
}

void HistoryManager::Undo() {
    if (m_undoStack.empty()) {
        std::cout << "[History] Undo: empty stack" << std::endl;
        return;
    }
    auto item = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    item->Undo();
    m_redoStack.push_back(std::move(item));
    std::cout << "[History] Undo done, stack=" << m_undoStack.size()
              << " redo=" << m_redoStack.size() << std::endl;
}

void HistoryManager::Redo() {
    if (m_redoStack.empty()) {
        std::cout << "[History] Redo: empty stack" << std::endl;
        return;
    }
    auto item = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    item->Redo();
    m_undoStack.push_back(std::move(item));
    std::cout << "[History] Redo done, stack=" << m_undoStack.size()
              << " redo=" << m_redoStack.size() << std::endl;
}

void HistoryManager::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

} // namespace VividPic
