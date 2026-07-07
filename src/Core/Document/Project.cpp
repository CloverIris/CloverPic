#include "Core/Document/Project.h"

namespace CloverPic {

Project::Project(const String& name) {
    m_metadata.name = name;
    m_metadata.createdAt = std::chrono::system_clock::now();
    m_metadata.modifiedAt = m_metadata.createdAt;
}

} // namespace CloverPic
