cmake_minimum_required(VERSION 3.25)
project(CloverPic VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The macOS adapter is written in Objective-C++ because it hosts the platform
# shell with AppKit. Only enable the language on Apple hosts so the existing
# Windows toolchain keeps working untouched.
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()

# MinGW specific settings
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Use a console subsystem for debug builds and a GUI subsystem for release.
# Passing both -mwindows and -mconsole makes MinGW emit conflicting subsystem
# options during linking on this machine.
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole -municode")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -municode")
    endif()
endif()

# Source files
file(GLOB_RECURSE CORE_SOURCES
    src/Core/*.cpp
)

file(GLOB_RECURSE CORE_HEADERS
    src/Core/*.h
    src/Utils/*.h
)

# The MVP refactor is driven by separate project-manager and workspace runtimes
# hosted by the platform adapter.
# CloverPicCore must stay platform-neutral; platform code enters only the executable.
add_library(CloverPicCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(CloverPicCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

if(WIN32)
    file(GLOB_RECURSE WINDOWS_ADAPTER_SOURCES
        src/Platform/Windows/*.cpp
    )

    file(GLOB_RECURSE WINDOWS_ADAPTER_HEADERS
        src/Platform/Windows/*.h
    )

    add_executable(CloverPic
        src/main.cpp
        ${WINDOWS_ADAPTER_SOURCES}
        ${WINDOWS_ADAPTER_HEADERS}
    )

    target_include_directories(CloverPic PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(CloverPic PRIVATE CloverPicCore)

    # Link Windows system libraries (MinGW compatible)
    target_link_libraries(CloverPic PRIVATE
        -Wl,-Bstatic
        winpthread
        -Wl,-Bdynamic
        user32
        gdi32
        gdiplus
        shell32
        ole32
        uuid
        comctl32
        dwmapi
        shlwapi
        winmm
        windowscodecs
    )

    # Enable Unicode
    target_compile_definitions(CloverPic PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)
elseif(APPLE)
    file(GLOB_RECURSE MAC_ADAPTER_SOURCES
        src/Platform/Mac/*.mm
        src/Platform/Mac/*.cpp
        src/Platform/Mac/*.h
    )

    add_executable(CloverPic
        ${MAC_ADAPTER_SOURCES}
    )

    target_include_directories(CloverPic PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(CloverPic PRIVATE CloverPicCore)

    # AppKit hosts the shell. CoreGraphics/ImageIO cover imaging, ColorSync the
    # display profiles, UniformTypeIdentifiers the file picker types.
    target_link_libraries(CloverPic PRIVATE
        "-framework Cocoa"
        "-framework AppKit"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
        "-framework ColorSync"
        "-framework UniformTypeIdentifiers"
        "-framework CoreServices"
        z
    )

    target_compile_options(CloverPic PRIVATE -fobjc-arc)

    # The workspace shell is a .cui document, not compiled code.
    target_compile_definitions(CloverPic PRIVATE
        CLOVERPIC_UI_DIR="${CMAKE_SOURCE_DIR}/src/Resource/ui")


    # CloverPlatform is a Rust submodule consumed through its C ABI. It is
    # optional: without cargo or the submodule the adapter still builds, just
    # without the experimental CloverPlatform surface.
    set(CLOVER_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/CloverPlatform")
    find_program(CARGO_EXECUTABLE cargo)
    if(CARGO_EXECUTABLE AND EXISTS "${CLOVER_PLATFORM_DIR}/Cargo.toml")
        set(CLOVER_ABI_LIB "${CLOVER_PLATFORM_DIR}/target/release/libclover_abi.a")
        add_custom_target(CloverPlatformAbi ALL
            COMMAND ${CARGO_EXECUTABLE} build --release
                    --manifest-path "${CLOVER_PLATFORM_DIR}/Cargo.toml" -p clover-abi
            BYPRODUCTS "${CLOVER_ABI_LIB}"
            COMMENT "Building the CloverPlatform C ABI (cargo)"
            VERBATIM)
        add_dependencies(CloverPic CloverPlatformAbi)
        target_include_directories(CloverPic PRIVATE "${CLOVER_PLATFORM_DIR}/include")
        target_compile_definitions(CloverPic PRIVATE CLOVERPIC_HAS_CLOVER_PLATFORM=1)
        target_link_libraries(CloverPic PRIVATE
            "${CLOVER_ABI_LIB}"
            "-framework Security"
            "-framework CoreFoundation")
        message(STATUS "CloverPlatform: enabled")
    else()
        message(STATUS "CloverPlatform: disabled (submodule or cargo not found)")
    endif()
else()
    message(FATAL_ERROR "CloverPic currently ships adapters for Windows and macOS only.")
endif()

# Copy assets to build directory when the repository includes them.
# Some clones only contain source code, so the build should not fail if
# the optional runtime assets directory is absent.
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET CloverPic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:CloverPic>/assets
    )
else()
    message(STATUS "Optional assets directory not found; skipping asset copy step.")
endif()
