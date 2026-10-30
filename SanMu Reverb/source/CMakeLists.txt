# ==============================================================================
# SanMuReverb source/CMakeLists.txt
# ==============================================================================

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

juce_add_plugin(SanMuReverb
    PLUGIN_MANUFACTURER_CODE "SmAu"
    PLUGIN_CODE              "SMRV"
    FORMATS                  ${SANMU_PLUGIN_FORMATS}
    PRODUCT_NAME             "SanMu Reverb"
    COMPANY_NAME             "SanMuAudio"
    AAX_CATEGORY             Reverb
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    NEEDS_WEB_BROWSER        TRUE
    ${SANMU_WEBVIEW2_ARGS}
    NEEDS_CURL               FALSE
)

# 完整包生成后再做本地签名，避免 AU/AAX 只带链接器的临时签名。
# 开发签名不替代正式分发所需的签名或 AAX 授权。
if(APPLE)
    foreach(format ${SANMU_PLUGIN_FORMATS})
        add_custom_command(TARGET SanMuReverb_${format} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:SanMuReverb_${format}>"
            VERBATIM)
    endforeach()
endif()
if(SANMU_COPY_PLUGIN_AFTER_BUILD)
    juce_enable_copy_plugin_step(SanMuReverb)
endif()

find_program(SANMU_NODE_EXECUTABLE node REQUIRED)
set(SANMU_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../prototypes/particle-life")
file(GLOB_RECURSE SANMU_UI_SOURCES CONFIGURE_DEPENDS
    "${SANMU_UI_ROOT}/src/*.js" "${SANMU_UI_ROOT}/src/*.css")
file(GLOB SANMU_WEB_TOOLS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web/*.mjs" "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web/*.js" "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web/*.css" "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/web/*.json")
set(SANMU_UI_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/sanmu-ui.zip")
add_custom_command(OUTPUT "${SANMU_UI_ARCHIVE}"
    COMMAND "${SANMU_NODE_EXECUTABLE}" "${SANMU_UI_ROOT}/scripts/build-native.mjs" "${SANMU_UI_ARCHIVE}"
    DEPENDS ${SANMU_UI_SOURCES} "${SANMU_UI_ROOT}/studio.html"
        "${SANMU_UI_ROOT}/scripts/build-native.mjs" "${SANMU_UI_ROOT}/package.json" ${SANMU_WEB_TOOLS}
    WORKING_DIRECTORY "${SANMU_UI_ROOT}"
    COMMENT "打包插件内置界面" VERBATIM)
set_source_files_properties("${SANMU_UI_ARCHIVE}" PROPERTIES GENERATED TRUE)
juce_add_binary_data(SanMuWebAssets NAMESPACE SanMuWebAssets HEADER_NAME SanMuWebAssets.h
    SOURCES "${SANMU_UI_ARCHIVE}")

target_sources(SanMuReverb
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        params/Parameters.cpp
        dsp/ReverbProcessor.cpp
        dsp/engines/PlateEngine.cpp
        dsp/engines/RoomEngine.cpp
        dsp/engines/HallEngine.cpp
        dsp/engines/ChamberEngine.cpp
        presets/PresetManager.cpp
)

target_compile_definitions(SanMuReverb
    PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
)
if(WIN32)
    target_compile_definitions(SanMuReverb PUBLIC JUCE_USE_WIN_WEBVIEW2=1)
endif()

target_link_libraries(SanMuReverb
    PRIVATE
        SanMuWebAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- 独立 DSP 自动化数值测试可执行文件 ----
add_executable(sanmureverb_test_dsp
    ../tests/test_reverb_dsp.cpp
    params/Parameters.cpp
    dsp/ReverbProcessor.cpp
    dsp/engines/PlateEngine.cpp
    dsp/engines/RoomEngine.cpp
    dsp/engines/HallEngine.cpp
    dsp/engines/ChamberEngine.cpp
    presets/PresetManager.cpp
)

target_include_directories(sanmureverb_test_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(sanmureverb_test_dsp
    PRIVATE
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
)

# 离线工具与契约测试只编译 DSP，不触碰已安装的插件。
function(sanmu_add_offline_target target main_file dsp_root)
    add_executable(${target}
        ${main_file}
        ${dsp_root}/dsp/ReverbProcessor.cpp
        ${dsp_root}/dsp/engines/PlateEngine.cpp
        ${dsp_root}/dsp/engines/RoomEngine.cpp
        ${dsp_root}/dsp/engines/HallEngine.cpp
        ${dsp_root}/dsp/engines/ChamberEngine.cpp
    )
    target_include_directories(${target} PRIVATE ${dsp_root})
    target_compile_definitions(${target} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(${target} PRIVATE juce::juce_dsp juce::juce_audio_formats
        juce::juce_cryptography juce::juce_recommended_config_flags)
endfunction()

sanmu_add_offline_target(sanmu_hall_render ../tools/render_hall.cpp ${CMAKE_CURRENT_SOURCE_DIR})
sanmu_add_offline_target(sanmu_switch_probe ../tools/probe_switching.cpp ${CMAKE_CURRENT_SOURCE_DIR})
if(BUILD_TESTING)
    add_test(NAME all_engines_regression COMMAND sanmureverb_test_dsp)
    sanmu_add_offline_target(sanmu_hall_contracts ../tests/test_hall_contracts.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME hall_contracts COMMAND sanmu_hall_contracts)
    sanmu_add_offline_target(sanmu_room_contracts ../tests/test_room_contracts.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME room_contracts COMMAND sanmu_room_contracts)
    sanmu_add_offline_target(sanmu_chamber_contracts ../tests/test_chamber_contracts.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME chamber_contracts COMMAND sanmu_chamber_contracts)
    sanmu_add_offline_target(sanmu_cathedral_contracts ../tests/test_cathedral_contracts.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME cathedral_contracts COMMAND sanmu_cathedral_contracts)
    sanmu_add_offline_target(sanmu_plate_contracts ../tests/test_plate_contracts.cpp ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME plate_contracts COMMAND sanmu_plate_contracts)
    add_test(NAME steel_contracts COMMAND sanmu_plate_contracts --steel)
    add_executable(sanmu_presets_test ../tests/test_presets.cpp)
    target_link_libraries(sanmu_presets_test PRIVATE SanMuReverb juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME presets_roundtrip COMMAND sanmu_presets_test)
    add_executable(sanmu_runtime_test ../tests/test_runtime.cpp)
    target_link_libraries(sanmu_runtime_test PRIVATE SanMuReverb juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME runtime_contracts COMMAND sanmu_runtime_test)
    add_executable(sanmu_native_ui_test ../tests/test_native_ui.cpp)
    target_compile_definitions(sanmu_native_ui_test PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(sanmu_native_ui_test PRIVATE SanMuReverb juce::juce_audio_utils juce::juce_gui_extra)
    add_test(NAME native_ui_contracts COMMAND sanmu_native_ui_test)
endif()

# 审计时可指定含 dsp/ 的历史快照目录，生成独立的修改前渲染器。
set(SANMU_BASELINE_DSP_ROOT "" CACHE PATH "可选的历史 DSP 快照根目录")
if(SANMU_BASELINE_DSP_ROOT)
    sanmu_add_offline_target(sanmu_hall_render_before ../tools/render_hall.cpp ${SANMU_BASELINE_DSP_ROOT})
endif()

# 真实编辑器的跨实例偏好验证，运行时必须使用隔离目录。
add_executable(sanmu_ui_preferences_test ../tests/test_ui_preferences.cpp)
target_link_libraries(sanmu_ui_preferences_test PRIVATE SanMuReverb juce::juce_audio_utils juce::juce_gui_extra)

# 通过隔离记录验证真实处理器到期路径，不修改系统时间或用户记录。
add_executable(sanmu_beta_processor_test ../tests/test_beta_processor.cpp)
target_include_directories(sanmu_beta_processor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sanmu_beta_processor_test PRIVATE SANMU_BETA_PROCESSOR=SanMuReverbAudioProcessor)
target_link_libraries(sanmu_beta_processor_test PRIVATE SanMuReverb juce::juce_audio_utils juce::juce_gui_extra)
