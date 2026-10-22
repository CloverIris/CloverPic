#pragma once

namespace CloverPic::Platform::Mac {

/// Opens an additive window that presents a CloverPlatform UI surface through its
/// C ABI, next to the two existing Win32-style core surfaces.
///
/// This is the integration spike: it proves that the macOS adapter can host the
/// Rust framework without touching the working `RgbaFrame` path. Enable it with
/// `CLOVERPIC_CLOVER_DEMO=1`. Returns false when the build has no CloverPlatform.
bool ShowCloverPlatformDemoWindow();

} // namespace CloverPic::Platform::Mac
