cmake_minimum_required(VERSION 3.24)
project(ayfxedit_wx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AYFX_REPO_ROOT "${CMAKE_SOURCE_DIR}/..")
set(AYFX_THIRD_PARTY_DIR "${AYFX_REPO_ROOT}/third_party")
set(DR_LIBS_DIR "${AYFX_THIRD_PARTY_DIR}/dr_libs")
set(MINIAUDIO_DIR "${AYFX_THIRD_PARTY_DIR}/miniaudio")
set(BLIP_AY_APU_DIR "${AYFX_THIRD_PARTY_DIR}/blip_ay_apu")
set(LH5_DIR "${AYFX_THIRD_PARTY_DIR}/lh5")

option(AYFX_USE_LOCAL_WX "Use wxWidgets from support/wxWidgets-3.3.3" ON)

set(wxWidgets_DIR "${AYFX_REPO_ROOT}/.local/wx/lib/cmake/wxWidgets-3.3" CACHE PATH "Path to wxWidgetsConfig.cmake directory")

find_package(wxWidgets CONFIG QUIET COMPONENTS core base html)
if(wxWidgets_FOUND)
    set(AYFX_WX_LIBS wxWidgets::wxWidgets)
else()
    if(AYFX_USE_LOCAL_WX)
        set(wxWidgets_ROOT_DIR "${CMAKE_SOURCE_DIR}/../support/wxWidgets-3.3.3")
    endif()

    find_package(wxWidgets REQUIRED COMPONENTS core base html)
    include(${wxWidgets_USE_FILE})
    set(AYFX_WX_LIBS ${wxWidgets_LIBRARIES})
endif()

if(NOT EXISTS "${MINIAUDIO_DIR}/miniaudio.h")
    message(FATAL_ERROR "miniaudio not found at ${MINIAUDIO_DIR}. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${DR_LIBS_DIR}/dr_wav.h")
    message(FATAL_ERROR "dr_libs not found at ${DR_LIBS_DIR}. Run: git submodule update --init --recursive")
endif()

add_executable(ayfxedit_wx WIN32
    app/main.cpp
    app/MainFrame.cpp
    app/MainFrame.h
    app/PianoWindow.cpp
    app/PianoWindow.h
    app/SettingsDialog.cpp
    app/SettingsDialog.h
    app/ClockPicker.cpp
    app/ClockPicker.h
    app/WindowGeometry.cpp
    app/WindowGeometry.h
    core/BankModel.h
    core/BankModel.cpp
    core/VtxDecoder.h
    core/VtxDecoder.cpp
    core/Settings.h
    core/Settings.cpp
    audio/AudioEngine.h
    audio/AudioEngine.cpp
    audio/WaveExport.h
    audio/WaveExport.cpp
    ${BLIP_AY_APU_DIR}/Ay_Apu.h
    ${BLIP_AY_APU_DIR}/Ay_Apu.cpp
    ${BLIP_AY_APU_DIR}/Blip_Buffer.h
    ${BLIP_AY_APU_DIR}/Blip_Buffer.cpp
    ${LH5_DIR}/ar.h
    ${LH5_DIR}/io.cpp
    ${LH5_DIR}/huf.cpp
    ${LH5_DIR}/maketbl.cpp
    ${LH5_DIR}/maketree.cpp
    ${LH5_DIR}/decode.cpp
)

if(WIN32)
    target_sources(ayfxedit_wx PRIVATE
        app/app_resources.rc
    )
endif()

target_include_directories(ayfxedit_wx PRIVATE ${MINIAUDIO_DIR} ${BLIP_AY_APU_DIR} ${DR_LIBS_DIR} ${LH5_DIR})

target_link_libraries(ayfxedit_wx PRIVATE
    ${AYFX_WX_LIBS}
)

if(MSVC)
    target_compile_definitions(ayfxedit_wx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    # <windows.h> (pulled in transitively by miniaudio.h) defines min/max
    # macros that otherwise break every std::min/std::max call in the file
    # that includes it, with a cryptic "illegal token on right side of ::".
    target_compile_definitions(ayfxedit_wx PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# miniaudio is header-only and statically compiled in; no DLL/shared lib to ship.
if(UNIX AND NOT APPLE)
    target_link_libraries(ayfxedit_wx PRIVATE dl pthread m)
elseif(APPLE)
    find_library(AYFX_COREFOUNDATION CoreFoundation REQUIRED)
    find_library(AYFX_COREAUDIO CoreAudio REQUIRED)
    find_library(AYFX_AUDIOTOOLBOX AudioToolbox REQUIRED)
    target_link_libraries(ayfxedit_wx PRIVATE
        ${AYFX_COREFOUNDATION} ${AYFX_COREAUDIO} ${AYFX_AUDIOTOOLBOX}
    )
endif()
