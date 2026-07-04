#pragma once

#include "Utils/Types.h"
#include <vector>
#include <memory>
#include <unordered_set>

namespace CloverPic {

class Layer;

class StrokeUndoItem {
public:
    struct TileSnapshot {
        uint32_t gridX = 0;
        uint32_t gridY = 0;
        std::vector<uint8_t> beforeData;
    };

    explicit StrokeUndoItem(Layer* layer);

    void CaptureTile(uint32_t gridX, uint32_t gridY, const uint8_t* data, size_t size);
    bool HasTile(uint32_t gridX, uint32_t gridY) const;
    bool IsEmpty() const { return m_snapshots.empty(); }
    size_t GetMemorySize() const;

    void CaptureRedoTiles();
    void Undo();
    void Redo();

private:
    Layer* m_layer = nullptr;
    std::vector<TileSnapshot> m_snapshots;      // before stroke
    std::vector<TileSnapshot> m_redoSnapshots;  // after stroke
    // Fast lookup: gridX | (gridY << 16)
    mutable std::unordered_set<uint32_t> m_capturedSet;
};

class HistoryManager {
public:
    HistoryManager() = default;

    void Push(std::unique_ptr<StrokeUndoItem> item);
    void Undo();
    void Redo();
    void Clear();

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    size_t GetUndoCount() const { return m_undoStack.size(); }
    size_t GetRedoCount() const { return m_redoStack.size(); }

    void SetMaxSteps(size_t max) { m_maxSteps = max; }
    size_t GetMaxSteps() const { return m_maxSteps; }

private:
    std::vector<std::unique_ptr<StrokeUndoItem>> m_undoStack;
    std::vector<std::unique_ptr<StrokeUndoItem>> m_redoStack;
    size_t m_maxSteps = 50;
};

} // namespace CloverPic
