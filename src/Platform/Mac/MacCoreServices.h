#pragma once

#include <functional>

namespace CloverPic::Platform::Mac {

// Registers the macOS implementation of every portable platform service that
// core consumes (memory/display facts, recent files, raw file bytes, PNG
// codec, file dialogs, font discovery, colour profiles, app settings).
//
// `activeWindowProvider` returns an opaque NSWindow* that modal dialogs should
// be parented to; it may return nullptr.
void RegisterMacCoreServices(std::function<void*()> activeWindowProvider);

// Directory where the adapter keeps app settings and the recent file list.
// Honours CLOVERPIC_DATA_DIR so tests can redirect it.
const char* MacDataDirectoryPath();

} // namespace CloverPic::Platform::Mac
