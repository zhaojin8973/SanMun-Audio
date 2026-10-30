# ==============================================================================
# SanMuDelay 插件目标与构建配置
# ==============================================================================

# ---- 设置 AAX SDK 路径 ----
option(SANMU_BUILD_AAX "在 Windows 上构建 AAX（需要 Windows AAX SDK）" OFF)
get_filename_component(SANMU_DEFAULT_AAX_SDK_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../../aax-sdk-2-9-0" ABSOLUTE)
set(SANMU_AAX_SDK_PATH "${SANMU_DEFAULT_AAX_SDK_PATH}" CACHE PATH
    "AAX SDK 根目录；仅构建 AAX 时需要，可通过 -D 指定")
set(SANMU_HAS_AAX_SDK FALSE)
if(EXISTS "${SANMU_AAX_SDK_PATH}/Interfaces/AAX.h")
    set(SANMU_HAS_AAX_SDK TRUE)
endif()
if((APPLE AND SANMU_HAS_AAX_SDK) OR (WIN32 AND SANMU_BUILD_AAX))
    if(NOT SANMU_HAS_AAX_SDK)
        message(FATAL_ERROR "未找到 AAX SDK，请通过 -DSANMU_AAX_SDK_PATH 指定本机 SDK 根目录")
    endif()
    juce_set_aax_sdk_path("${SANMU_AAX_SDK_PATH}")
elseif(APPLE)
    message(STATUS "未检测到本机 AAX SDK (${SANMU_AAX_SDK_PATH})，本次仅构建 VST3 与 AU 格式")
endif()
set(SANMU_PLUGIN_FORMATS VST3)
if(APPLE)
    set(SANMU_PLUGIN_FORMATS VST3 AU)
    if(SANMU_HAS_AAX_SDK)
        list(APPEND SANMU_PLUGIN_FORMATS AAX)
    endif()
elseif(WIN32 AND SANMU_BUILD_AAX)
    list(APPEND SANMU_PLUGIN_FORMATS AAX)
endif()
set(SANMU_WEBVIEW2_ARGS)
if(WIN32)
    # Windows 版界面使用 Chromium WebView2；需要预先安装对应 NuGet SDK。
    list(APPEND SANMU_WEBVIEW2_ARGS NEEDS_WEBVIEW2 TRUE)
endif()

# ---- 定义插件目标（元数据 + 格式） ----
option(SANMU_COPY_PLUGIN_AFTER_BUILD "构建后安装插件" ON)
juce_add_plugin(SanMuDelay
    PLUGIN_MANUFACTURER_CODE "SmAu"      # 4 字符制造商代码
    PLUGIN_CODE              "SMD1"      # 4 字符插件代码（延迟器）
    FORMATS                  ${SANMU_PLUGIN_FORMATS}
    PRODUCT_NAME             "SanMu Delay"
    COMPANY_NAME             "SanMuAudio"
    AAX_CATEGORY             Delay
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  ${SANMU_COPY_PLUGIN_AFTER_BUILD}
    NEEDS_WEB_BROWSER        TRUE
    ${SANMU_WEBVIEW2_ARGS}
    NEEDS_CURL               FALSE
)

# 完全离线打包，与 Reverb / Comp 使用同一网页视图及系列控件样式
find_program(SANMU_NODE_EXECUTABLE node REQUIRED)
set(SANMU_WEB_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web")
file(GLOB_RECURSE SANMU_WEB_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../ui/*")
file(GLOB SANMU_WEB_TOOLS CONFIGURE_DEPENDS "${SANMU_WEB_TOOL_DIR}/*.mjs" "${SANMU_WEB_TOOL_DIR}/*.js" "${SANMU_WEB_TOOL_DIR}/*.css" "${SANMU_WEB_TOOL_DIR}/*.json")
set(SANMU_WEB_ZIP "${CMAKE_CURRENT_BINARY_DIR}/sanmu-delay-ui.zip")
add_custom_command(OUTPUT "${SANMU_WEB_ZIP}"
    COMMAND "${SANMU_NODE_EXECUTABLE}" "${SANMU_WEB_TOOL_DIR}/build-ui.mjs"
        "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${SANMU_WEB_ZIP}"
    DEPENDS ${SANMU_WEB_FILES} ${SANMU_WEB_TOOLS} VERBATIM)
juce_add_binary_data(SanMuDelayWebAssets HEADER_NAME SanMuDelayWebAssets.h NAMESPACE SanMuDelayWebAssets SOURCES "${SANMU_WEB_ZIP}")
target_link_libraries(SanMuDelay PRIVATE SanMuDelayWebAssets)

# ---- 添加源文件到插件目标 ----
target_sources(SanMuDelay
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        presets/PresetManager.cpp
        dsp/DelayProcessor.cpp
        dsp/engines/ModernEngine.cpp
        dsp/engines/TapeEngine.cpp
        dsp/engines/BbdEngine.cpp
        dsp/engines/DiffusedEngine.cpp
        params/Parameters.cpp
)

# ---- 编译定义 ----
target_compile_definitions(SanMuDelay
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
if(WIN32)
    target_compile_definitions(SanMuDelay PUBLIC JUCE_USE_WIN_WEBVIEW2=1)
endif()

# ---- 链接 JUCE 模块 ----
target_link_libraries(SanMuDelay
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- DSP 物理建模与稳定性单元测试 ----
add_executable(test_delay_dsp
    ../tests/test_delay_dsp.cpp
    dsp/DelayProcessor.cpp
    dsp/engines/ModernEngine.cpp
    dsp/engines/TapeEngine.cpp
    dsp/engines/BbdEngine.cpp
    dsp/engines/DiffusedEngine.cpp
    params/Parameters.cpp
)

target_include_directories(test_delay_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_delay_dsp
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# ---- UI 架构集成测试 ----
add_executable(test_delay_ui
    ../tests/test_delay_ui.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    presets/PresetManager.cpp
    dsp/DelayProcessor.cpp
    dsp/engines/ModernEngine.cpp
    dsp/engines/TapeEngine.cpp
    dsp/engines/BbdEngine.cpp
    dsp/engines/DiffusedEngine.cpp
    params/Parameters.cpp
)
target_include_directories(test_delay_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_delay_ui
    PRIVATE
        SanMuDelayWebAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)


# 原生网页桥接回归需要驱动消息循环，Release 中检查仍有效。
target_compile_definitions(test_delay_ui PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
add_test(NAME SanMuDelay_DSP COMMAND test_delay_dsp)
add_test(NAME SanMuDelay_UI COMMAND test_delay_ui)

# 真实编辑器的跨实例偏好验证，运行时必须使用隔离目录。
add_executable(sanmu_ui_preferences_test ../tests/test_ui_preferences.cpp)
target_link_libraries(sanmu_ui_preferences_test PRIVATE SanMuDelay juce::juce_audio_utils juce::juce_gui_extra)

# 通过隔离记录验证真实处理器到期路径，不修改系统时间或用户记录。
add_executable(sanmu_beta_processor_test ../tests/test_beta_processor.cpp)
target_include_directories(sanmu_beta_processor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmu_beta_processor_test PRIVATE SANMU_BETA_PROCESSOR=SanMuDelayAudioProcessor)
target_link_libraries(sanmu_beta_processor_test PRIVATE SanMuDelay juce::juce_audio_utils juce::juce_gui_extra)
