#include "Core/Layer.h"

namespace CloverPic {

Layer::Layer(const String& name, LayerType type, uint32_t canvasWidth, uint32_t canvasHeight)
    : m_name(name), m_type(type), m_canvasWidth(canvasWidth), m_canvasHeight(canvasHeight) {
    InitializeGrid();
}

void Layer::InitializeGrid() {
    m_gridWidth = (m_canvasWidth + Render::TILE_SIZE - 1) / Render::TILE_SIZE;
    m_gridHeight = (m_canvasHeight + Render::TILE_SIZE - 1) / Render::TILE_SIZE;
}

} // namespace CloverPic
