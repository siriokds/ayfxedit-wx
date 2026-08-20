cmake_minimum_required(VERSION 3.24)
project(ayfxedit_wx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AYFX_REPO_ROOT "${CMAKE_SOURCE_DIR}/..")
set(AYFX_THIRD_PARTY_DIR "${AYFX_REPO_ROOT}/third_party")
set(DR_LIBS_DIR "${AYFX_THIRD_PARTY_DIR}/dr_libs")

option(AYFX_USE_LOCAL_WX "Use wxWidgets from support/wxWidgets-3.3.3" ON)

set(wxWidgets_DIR "${AYFX_REPO_ROOT}/.local/wx/lib/cmake/wxWidgets-3.3" CACHE PATH "Path to wxWidgetsConfig.cmake directory")

find_package(wxWidgets CONFIG QUIET COMPONENTS core base)
if(wxWidgets_FOUND)
    set(AYFX_WX_LIBS wxWidgets::wxWidgets)
else()
    if(AYFX_USE_LOCAL_WX)
        set(wxWidgets_ROOT_DIR "${CMAKE_SOURCE_DIR}/../support/wxWidgets-3.3.3")
    endif()

    find_package(wxWidgets REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    set(AYFX_WX_LIBS ${wxWidgets_LIBRARIES})
endif()

# Try the modern SDL3 CMake config first (works on Linux/macOS and Windows with vcpkg/cmake install)
find_package(SDL3 CONFIG QUIET)

if(SDL3_FOUND)
    set(SDL3_LIBRARY SDL3::SDL3)
    set(SDL3_INCLUDE_DIR "")  # handled by the imported target
else()
    # Fallback: manual search; on Windows default to C:/SDL3
    if(WIN32)
        set(SDL3_ROOT "C:/SDL3" CACHE PATH "Path to SDL3 root (Windows fallback)")
    else()
        set(SDL3_ROOT "" CACHE PATH "Path to SDL3 root")
    endif()
    find_path(SDL3_INCLUDE_DIR SDL3/SDL.h
        PATHS "${SDL3_ROOT}/include" /usr/include /usr/local/include)
    find_library(SDL3_LIBRARY SDL3
        PATHS "${SDL3_ROOT}/lib/x64" "${SDL3_ROOT}/lib" /usr/lib /usr/local/lib)
    if(NOT SDL3_INCLUDE_DIR OR NOT SDL3_LIBRARY)
        message(FATAL_ERROR "SDL3 not found. Install SDL3 or set SDL3_ROOT.")
    endif()
endif()

add_executable(ayfxedit_wx WIN32
    app/main.cpp
    app/MainFrame.cpp
    app/MainFrame.h
    core/BankModel.h
    core/BankModel.cpp
    audio/AudioEngine.h
    audio/AudioEngine.cpp
    audio/AY8910.h
    audio/AY8910.cpp
)

if(WIN32)
    target_sources(ayfxedit_wx PRIVATE
        app/app_resources.rc
    )
endif()

if(SDL3_INCLUDE_DIR)
    target_include_directories(ayfxedit_wx PRIVATE ${SDL3_INCLUDE_DIR})
endif()

if(EXISTS "${DR_LIBS_DIR}/dr_wav.h")
    target_include_directories(ayfxedit_wx PRIVATE
        ${DR_LIBS_DIR}
    )
    message(STATUS "Using dr_libs from ${DR_LIBS_DIR}")
else()
    message(STATUS "dr_libs not found at ${DR_LIBS_DIR}; continuing without it")
endif()

target_link_libraries(ayfxedit_wx PRIVATE
    ${AYFX_WX_LIBS}
    ${SDL3_LIBRARY}
)

if(MSVC)
    target_compile_definitions(ayfxedit_wx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

find_file(SDL3_DLL SDL3.dll PATHS "${SDL3_ROOT}/lib/x64" "${SDL3_ROOT}/lib" "${SDL3_ROOT}")
if(SDL3_DLL)
    add_custom_command(TARGET ayfxedit_wx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL3_DLL}" "$<TARGET_FILE_DIR:ayfxedit_wx>"
        COMMENT "Copying SDL3.dll"
    )
endif()
