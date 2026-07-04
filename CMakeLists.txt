cmake_minimum_required(VERSION 3.25)
project(CloverPic VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MinGW specific settings
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

# Use a console subsystem for debug builds and a GUI subsystem for release.
# Passing both -mwindows and -mconsole makes MinGW emit conflicting subsystem
# options during linking on this machine.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole -municode")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -municode")
endif()

# Source files
file(GLOB_RECURSE CORE_SOURCES
    src/Core/*.cpp
)

file(GLOB_RECURSE CORE_HEADERS
    src/Core/*.h
    src/Utils/*.h
)

file(GLOB_RECURSE WINDOWS_ADAPTER_SOURCES
    src/Platform/Windows/*.cpp
)

file(GLOB_RECURSE WINDOWS_ADAPTER_HEADERS
    src/Platform/Windows/*.h
)

# The MVP refactor is driven by separate project-manager and workspace runtimes
# hosted by Platform/Windows/WindowsHost.
# CloverPicCore must stay platform-neutral; Windows-only code enters only the executable.
add_library(CloverPicCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(CloverPicCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
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

