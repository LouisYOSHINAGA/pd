cmake_minimum_required(VERSION 3.19.6)

# ---------------------------------------------------------------------------
set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Libs/VST_SDK/vst3sdk")

if(NOT EXISTS "${vst3sdk_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "VST3 SDK が見つかりません: ${vst3sdk_SOURCE_DIR}\n"
        "vst3sdk_SOURCE_DIR のパスを実際の配置に合わせて修正してください。"
    )
endif()

set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)

project(pd
    VERSION 0.0.0
    DESCRIPTION "PD VST3 Plug-in"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${vst3sdk_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

# ---------------------------------------------------------------------------
smtg_add_vst3plugin(pd
    config.h
    config.cpp
    const.h
    pd.h
    pd.cpp
    processor.h
    processor.cpp
    controller.h
    controller.cpp
    editor.h
    editor.cpp
    eg.h
    eg.cpp
    voice.h
    voice.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.h
)

target_link_libraries(pd
    PRIVATE
        sdk
        base
        vstgui_support
)

smtg_target_configure_version_file(pd)
