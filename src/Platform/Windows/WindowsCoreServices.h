#pragma once

#include <windows.h>
#include <functional>

namespace CloverPic {

void RegisterWindowsCoreServices(std::function<HWND()> ownerProvider);

} // namespace CloverPic
