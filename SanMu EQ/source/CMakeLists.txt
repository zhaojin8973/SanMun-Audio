# ==============================================================================
# SanMuQ6 插件目标定义
# 参考 JUCE 9 官方示例 CMakeLists.txt 的结构
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
juce_add_plugin(SanMuQ6
    PLUGIN_MANUFACTURER_CODE "SmAu"      # 4 字符制造商代码
    PLUGIN_CODE              "SMQ6"      # 4 字符插件代码
    FORMATS                  ${SANMU_PLUGIN_FORMATS}
    PRODUCT_NAME             "SanMu EQ6"
    COMPANY_NAME             "SanMuAudio"
    AAX_CATEGORY             EQ
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE       # 完成验证与 AAX 签名后再覆盖安装，避免发布半成品
    NEEDS_WEB_BROWSER        TRUE
    ${SANMU_WEBVIEW2_ARGS}
    NEEDS_CURL               FALSE
)

# ---- 添加源文件到插件目标 ----
target_sources(SanMuQ6
    PRIVATE
        PluginProcessor.cpp
        WebEqEditor.cpp
        dsp/EqBand.cpp
        dsp/EqProcessor.cpp
        dsp/LinearPhaseEq.cpp
        dsp/PrecisionLinearEq.cpp
        params/Parameters.cpp
        presets/PresetManager.cpp
)

add_executable(sanmu_precision_tests ../tests/test_precision.cpp dsp/PrecisionLinearEq.cpp)
target_include_directories(sanmu_precision_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmu_precision_tests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

# ---- 编译定义 ----
target_compile_definitions(SanMuQ6
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0    # 新插件无 VST2 历史
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
if(WIN32)
    target_compile_definitions(SanMuQ6 PUBLIC JUCE_USE_WIN_WEBVIEW2=1)
endif()

find_program(SANMU_NODE_EXECUTABLE node REQUIRED)
set(SANMU_WEB_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web")
file(GLOB_RECURSE SANMU_WEB_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../ui/*")
file(GLOB SANMU_WEB_TOOLS CONFIGURE_DEPENDS "${SANMU_WEB_TOOL_DIR}/*.mjs" "${SANMU_WEB_TOOL_DIR}/*.js" "${SANMU_WEB_TOOL_DIR}/*.css" "${SANMU_WEB_TOOL_DIR}/*.json")
set(SANMU_EQ_WEB_ZIP "${CMAKE_CURRENT_BINARY_DIR}/sanmuequi.zip")
add_custom_command(OUTPUT "${SANMU_EQ_WEB_ZIP}"
    COMMAND "${SANMU_NODE_EXECUTABLE}" "${SANMU_WEB_TOOL_DIR}/build-ui.mjs"
        "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${SANMU_EQ_WEB_ZIP}"
    DEPENDS ${SANMU_WEB_FILES} ${SANMU_WEB_TOOLS} VERBATIM)
juce_add_binary_data(SanMuEqWebAssets HEADER_NAME SanMuEqWebAssets.h NAMESPACE SanMuEqWebAssets SOURCES "${SANMU_EQ_WEB_ZIP}")
target_link_libraries(SanMuQ6 PRIVATE SanMuEqWebAssets)

# ---- 链接 JUCE 模块 ----
target_link_libraries(SanMuQ6
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ==============================================================================
# 独立数值验证（非插件，用于诊断线性相位 OLA 大增益滋滋声）
# ==============================================================================
add_executable(sanmuq6_test_ola
    ../tests/test_linearphase.cpp
    dsp/LinearPhaseEq.cpp
    dsp/PrecisionLinearEq.cpp
)
target_include_directories(sanmuq6_test_ola PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_ola
    PRIVATE juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags
)

# 临时验证：插件完整路径 LinearPhase 有声性
add_executable(sanmuq6_test_plugin_path
    ../tests/test_linearphase_plugin_path.cpp
    dsp/EqProcessor.cpp
    dsp/EqBand.cpp
    dsp/LinearPhaseEq.cpp
    dsp/PrecisionLinearEq.cpp
)
target_include_directories(sanmuq6_test_plugin_path PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_plugin_path
    PRIVATE juce::juce_dsp juce::juce_graphics
    PUBLIC  juce::juce_recommended_config_flags
)

# 图标路径解析验证（官方 SVG path 数据 → juce::Path）
add_executable(sanmuq6_test_icons
    ../tests/test_iconpaths.cpp
)
target_include_directories(sanmuq6_test_icons PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_icons
    PRIVATE juce::juce_gui_basics
    PUBLIC  juce::juce_recommended_config_flags
)

# 品牌字体匹配诊断（验证 "Avenir Next Medium" 等族名能否解析）
add_executable(sanmuq6_test_fonts
    ../tests/test_fonts.cpp
)
target_include_directories(sanmuq6_test_fonts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_fonts
    PRIVATE juce::juce_gui_basics
    PUBLIC  juce::juce_recommended_config_flags
)

# M0/M1 回归测试与契约验收（R01~R08 与 226 参数布局）
add_executable(sanmu_eq_regression_tests
    ../tests/test_regression.cpp
    PluginProcessor.cpp
    dsp/EqProcessor.cpp
    dsp/EqBand.cpp
    dsp/LinearPhaseEq.cpp
    dsp/PrecisionLinearEq.cpp
    params/Parameters.cpp
    presets/PresetManager.cpp
)
target_include_directories(sanmu_eq_regression_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmu_eq_regression_tests PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0 SANMU_NO_EDITOR_TEST=1)
target_link_libraries(sanmu_eq_regression_tests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)

# 原生桥接契约验证与内嵌网页交互预览
add_executable(sanmuq6_render_ui
    ../tests/test_render_ui.cpp
    PluginProcessor.cpp
    WebEqEditor.cpp
    dsp/EqProcessor.cpp
    dsp/EqBand.cpp
    dsp/LinearPhaseEq.cpp
    dsp/PrecisionLinearEq.cpp
    params/Parameters.cpp
    presets/PresetManager.cpp
)
target_include_directories(sanmuq6_render_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmuq6_render_ui PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_WEB_BROWSER=1)
target_link_libraries(sanmuq6_render_ui
    PRIVATE
        SanMuEqWebAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
)

# 真实编辑器的跨实例偏好验证，运行时必须使用隔离目录。
add_executable(sanmu_ui_preferences_test ../tests/test_ui_preferences.cpp)
target_link_libraries(sanmu_ui_preferences_test PRIVATE SanMuQ6 juce::juce_audio_utils juce::juce_gui_extra)

# 内测期限与无分配音频门控验证。
add_executable(sanmu_beta_test ../tests/test_beta.cpp)
target_include_directories(sanmu_beta_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmu_beta_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

# 通过隔离记录验证真实处理器到期路径，不修改系统时间或用户记录。
add_executable(sanmu_beta_processor_test ../tests/test_beta_processor.cpp)
target_include_directories(sanmu_beta_processor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmu_beta_processor_test PRIVATE SANMU_BETA_PROCESSOR=SanMuQ6AudioProcessor)
target_link_libraries(sanmu_beta_processor_test PRIVATE SanMuQ6 juce::juce_audio_utils juce::juce_gui_extra)
