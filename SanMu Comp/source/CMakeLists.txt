# ==============================================================================
# SanMuComp 插件目标定义
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
option(SANMU_COPY_PLUGIN_AFTER_BUILD "构建后安装插件" ON)
juce_add_plugin(SanMuComp
    PLUGIN_MANUFACTURER_CODE "SmAu"      # 4 字符制造商代码
    PLUGIN_CODE              "SMC1"      # 4 字符插件代码（压缩器）
    FORMATS                  ${SANMU_PLUGIN_FORMATS}
    PRODUCT_NAME             "SanMu Comp"
    COMPANY_NAME             "SanMuAudio"
    AAX_CATEGORY             Dynamics
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

# 完全离线打包，与 Reverb 使用同一网页视图及系列控件样式。
find_program(SANMU_NODE_EXECUTABLE node REQUIRED)
set(SANMU_WEB_TOOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web")
file(GLOB_RECURSE SANMU_WEB_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../ui/*")
file(GLOB SANMU_WEB_TOOLS CONFIGURE_DEPENDS "${SANMU_WEB_TOOL_DIR}/*.mjs" "${SANMU_WEB_TOOL_DIR}/*.js" "${SANMU_WEB_TOOL_DIR}/*.css" "${SANMU_WEB_TOOL_DIR}/*.json")
set(SANMU_WEB_ZIP "${CMAKE_CURRENT_BINARY_DIR}/sanmu-comp-ui.zip")
add_custom_command(OUTPUT "${SANMU_WEB_ZIP}"
    COMMAND "${SANMU_NODE_EXECUTABLE}" "${SANMU_WEB_TOOL_DIR}/build-ui.mjs"
        "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${SANMU_WEB_ZIP}"
    DEPENDS ${SANMU_WEB_FILES} ${SANMU_WEB_TOOLS} VERBATIM)
juce_add_binary_data(SanMuCompWebAssets HEADER_NAME SanMuCompWebAssets.h NAMESPACE SanMuCompWebAssets SOURCES "${SANMU_WEB_ZIP}")
target_link_libraries(SanMuComp PRIVATE SanMuCompWebAssets)

# ---- 添加源文件到插件目标 ----
target_sources(SanMuComp
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        dsp/CompressorProcessor.cpp
        dsp/AcousticFeatureExtractor.cpp
        dsp/engines/CleanEngine.cpp
        dsp/engines/VoxEngine.cpp
        dsp/engines/VcaEngine.cpp
        dsp/engines/FetEngine.cpp
        dsp/engines/OptoEngine.cpp
        ai/LlmClient.cpp
        params/Parameters.cpp
        presets/PresetManager.cpp
)

# ---- 编译定义 ----
target_compile_definitions(SanMuComp
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0    # 新插件无 VST2 历史
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
if(WIN32)
    target_compile_definitions(SanMuComp PUBLIC JUCE_USE_WIN_WEBVIEW2=1)
endif()

# ---- 链接 JUCE 模块 ----
target_link_libraries(SanMuComp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- AAX 自动签名 ----
option(SANMU_SIGN_AAX_AFTER_BUILD "使用发布环境凭据签名 AAX 构建产物" OFF)
if(APPLE AND TARGET SanMuComp_AAX AND SANMU_SIGN_AAX_AFTER_BUILD)
    add_custom_command(TARGET SanMuComp_AAX POST_BUILD
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/sign_aax.sh" "$<TARGET_BUNDLE_DIR:SanMuComp_AAX>"
        VERBATIM
        COMMENT "Auto signing AAX plugin with PACE Eden wraptool..."
    )
endif()

# ---- DSP 物理建模与稳定性单元测试 ----
# 旧绘制器仅供历史块时序审计，正式插件完全使用新网页界面。
add_executable(test_plugin_integration ../tests/test_plugin_integration.cpp ui/InputLevelMeter.cpp ui/LookAndFeel.cpp)
target_include_directories(test_plugin_integration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_plugin_integration PRIVATE SanMuComp juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)

add_executable(test_compressor_dsp
    ../tests/test_compressor_dsp.cpp
    dsp/CompressorProcessor.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/CleanEngine.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_compressor_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_compressor_dsp
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# 图标路径解析验证
add_executable(sanmuq6_test_icons
    ../tests/test_iconpaths.cpp
)
target_include_directories(sanmuq6_test_icons PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_icons
    PRIVATE juce::juce_gui_basics
    PUBLIC  juce::juce_recommended_config_flags
)

# 品牌字体匹配诊断
add_executable(sanmuq6_test_fonts
    ../tests/test_fonts.cpp
)
target_include_directories(sanmuq6_test_fonts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sanmuq6_test_fonts
    PRIVATE juce::juce_gui_basics
    PUBLIC  juce::juce_recommended_config_flags
)

# AI 声学多模态提取器测试
add_executable(test_ai_feature_extractor
    ../tests/test_ai_feature_extractor.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/CleanEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_ai_feature_extractor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_ai_feature_extractor
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# 本地声学推荐物理方程组测试
add_executable(test_local_acoustic_recommender
    ../tests/test_local_acoustic_recommender.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/CleanEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_local_acoustic_recommender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_local_acoustic_recommender
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# FFT 频谱质心与乐器分类测试
add_executable(test_fft_spectral_classifier
    ../tests/test_fft_spectral_classifier.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/CleanEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_fft_spectral_classifier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_fft_spectral_classifier
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# 真实音频虚拟 DSP 闭环等峰值/等响度校准测试
add_executable(test_virtual_dsp_calibration
    ../tests/test_virtual_dsp_calibration.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/CleanEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_virtual_dsp_calibration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_virtual_dsp_calibration
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

# 6. AI 智能混音副驾驶多轮对话与行话增量单元测试
add_executable(test_conversational_recommender
    ../tests/test_conversational_recommender.cpp
    dsp/AcousticFeatureExtractor.cpp
    dsp/engines/VoxEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/CleanEngine.cpp
    ai/LlmClient.cpp
    params/Parameters.cpp
)
target_include_directories(test_conversational_recommender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_conversational_recommender
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
)

add_executable(test_dynamic_behavior
    ../tests/test_dynamic_behavior.cpp
    dsp/engines/FetEngine.cpp
    dsp/engines/VcaEngine.cpp
    dsp/engines/OptoEngine.cpp
    dsp/engines/VoxEngine.cpp
)
target_include_directories(test_dynamic_behavior PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_dynamic_behavior PRIVATE juce::juce_dsp juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags)

foreach(test_target IN ITEMS test_dynamic_behavior test_plugin_integration test_compressor_dsp test_ai_feature_extractor
        test_local_acoustic_recommender test_fft_spectral_classifier test_virtual_dsp_calibration
        test_conversational_recommender)
    target_compile_options(${test_target} PRIVATE -UNDEBUG)
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

# 私有分轨只通过本地路径读取，不纳入仓库。
add_executable(listen_material_probe ../tests/listen_material_probe.cpp)
target_include_directories(listen_material_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(listen_material_probe PRIVATE SanMuComp juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)

add_executable(test_listen_assistant ../tests/test_listen_assistant.cpp)
target_include_directories(test_listen_assistant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_listen_assistant PRIVATE SanMuComp juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)
add_test(NAME test_listen_assistant COMMAND test_listen_assistant "${CMAKE_CURRENT_BINARY_DIR}/listen-applied.png")

# 系列 UI：验证真实原生组件、曲线交互与实例主题，不涉及宿主听测。
add_executable(test_ui_redesign ../tests/test_ui_redesign.cpp)
target_include_directories(test_ui_redesign PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_ui_redesign PRIVATE SanMuComp juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)
target_compile_definitions(test_ui_redesign PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
add_test(NAME ui_redesign COMMAND test_ui_redesign)

# 真实编辑器的跨实例偏好验证，运行时必须使用隔离目录。
add_executable(sanmu_ui_preferences_test ../tests/test_ui_preferences.cpp)
target_link_libraries(sanmu_ui_preferences_test PRIVATE SanMuComp juce::juce_audio_utils juce::juce_gui_extra)

# 通过隔离记录验证真实处理器到期路径，不修改系统时间或用户记录。
add_executable(sanmu_beta_processor_test ../tests/test_beta_processor.cpp)
target_include_directories(sanmu_beta_processor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmu_beta_processor_test PRIVATE SANMU_BETA_PROCESSOR=SanMuCompAudioProcessor)
target_link_libraries(sanmu_beta_processor_test PRIVATE SanMuComp juce::juce_audio_utils juce::juce_gui_extra)
