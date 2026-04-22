#include "Core/RecentFilesManager.h"
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <windows.h>

namespace VividPic {

RecentFilesManager& RecentFilesManager::GetInstance() {
    static RecentFilesManager instance;
    return instance;
}

RecentFilesManager::RecentFilesManager() {
    Load();
}

String RecentFilesManager::GetStoragePath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        String result = path;
        result += L"\\VividPic";
        CreateDirectoryW(result.c_str(), nullptr);
        result += L"\\recent.txt";
        return result;
    }
    return L"";
}

void RecentFilesManager::Load() {
    String path = GetStoragePath();
    if (path.empty()) return;
    
    std::wifstream file(path.c_str());
    if (!file.is_open()) return;
    
    String line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            m_files.push_back(line);
        }
    }
}

void RecentFilesManager::Save() {
    String path = GetStoragePath();
    if (path.empty()) return;
    
    std::wofstream file(path.c_str());
    if (!file.is_open()) return;
    
    for (const auto& f : m_files) {
        file << f << L"\n";
    }
}

void RecentFilesManager::AddRecentFile(const String& path) {
    auto it = std::find(m_files.begin(), m_files.end(), path);
    if (it != m_files.end()) {
        m_files.erase(it);
    }
    
    m_files.insert(m_files.begin(), path);
    
    if (m_files.size() > MaxRecentFiles) {
        m_files.resize(MaxRecentFiles);
    }
    
    Save();
}

void RecentFilesManager::Clear() {
    m_files.clear();
    Save();
}

} // namespace VividPic
