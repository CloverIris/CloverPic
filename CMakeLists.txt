cmake_minimum_required(VERSION 3.25)
project(VividPic VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MinGW specific settings
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -mwindows")
endif()

# Windows subsystem for release, console for debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole -municode")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -municode")
endif()

# Source files
file(GLOB_RECURSE SOURCES 
    src/*.cpp
)

file(GLOB_RECURSE HEADERS
    src/*.h
)

add_executable(VividPic ${SOURCES} ${HEADERS})

target_include_directories(VividPic PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)

# Link Windows system libraries (MinGW compatible)
target_link_libraries(VividPic PRIVATE
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
    d2d1
    dwrite
    windowscodecs
)

# Enable Unicode
target_compile_definitions(VividPic PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)

# Copy assets to build directory
add_custom_command(TARGET VividPic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:VividPic>/assets
)

# Copy MinGW runtime DLLs for standalone execution
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET VividPic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN_DIR}/libwinpthread-1.dll" $<TARGET_FILE_DIR:VividPic>
    )
endif()
