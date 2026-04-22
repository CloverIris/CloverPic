#pragma once

#include "Utils/Types.h"
#include <vector>
#include <algorithm>

namespace VividPic {

class RecentFilesManager {
public:
    static RecentFilesManager& GetInstance();
    
    void AddRecentFile(const String& path);
    const std::vector<String>& GetRecentFiles() const { return m_files; }
    void Clear();
    
private:
    RecentFilesManager();
    
    String GetStoragePath() const;
    void Load();
    void Save();
    
    std::vector<String> m_files;
    static constexpr size_t MaxRecentFiles = 10;
};

} // namespace VividPic
