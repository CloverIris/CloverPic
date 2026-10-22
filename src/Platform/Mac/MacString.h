#pragma once

#include "Utils/Types.h"
#include <string>

namespace CloverPic::Platform::Mac {

// Core owns `String` as std::wstring. On Windows that is UTF-16; on Apple
// platforms wchar_t is 32 bit wide. Core also decodes UTF-16BE name records
// into individual 16 bit code units, so the adapter normalizes the boundary:
// conversions back into `String` always produce UTF-16 code units, which keeps
// core's UTF-16 based text paths working unchanged.
std::string ToUtf8(const String& text);
String FromUtf8(const std::string& text);

// Converts a core path to a filesystem path usable with POSIX APIs.
std::string ToFilesystemPath(const String& path);

} // namespace CloverPic::Platform::Mac
