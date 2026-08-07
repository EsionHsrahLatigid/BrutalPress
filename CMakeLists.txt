cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(BrutalPress VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(BrutalPress VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(BRUTALPRESS_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(BRUTALPRESS_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(BRUTALPRESS_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(BRUTALPRESS_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")
set(BRUTALPRESS_CREATE_AU OFF)
if(APPLE)
    set(BRUTALPRESS_CREATE_AU ON)
endif()

add_library(brutalpress_engine STATIC
    source/violent/BrutalPressEngine.cpp)
target_include_directories(brutalpress_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(brutalpress_engine PUBLIC cxx_std_20)
set_target_properties(brutalpress_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(brutalpress_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BRUTALPRESS_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(brutalpress_engine_tests
        tests/BrutalPressEngineTests.cpp)
    target_link_libraries(brutalpress_engine_tests PRIVATE brutalpress_engine)
    target_compile_features(brutalpress_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(brutalpress_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(brutalpress_engine_tests)
    add_test(NAME brutalpress_engine_tests COMMAND brutalpress_engine_tests)
endif()

if(BRUTALPRESS_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${BRUTALPRESS_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${BRUTALPRESS_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME brutalpress
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "BrutalPress"
        TARGET_APP_ID "audio.2bit.brutalpress"
        TARGET_APP_NAMESPACE "audio.2bit"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "audio.2bit.BrutalPress"
        PLUGIN_NAME "BrutalPress"
        PLUGIN_VENDOR "2bit"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Extreme three-band upward and downward compressor"
        PLUGIN_URL "https://github.com/2bbb/BrutalPress"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${BRUTALPRESS_CREATE_AU}
        PLUGIN_AU_SUBTYPE "BrPr"
        PLUGIN_AU_MANUFACTURER "2Bit"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(brutalpress_shared INTERFACE
        source/violent/ParameterGridEditor.cpp
        source/violent/ParameterGridEditor.h
        source/violent/ProductState.h
        source/violent/plugins/BrutalPressPlugin.cpp
        source/violent/plugins/BrutalPressPlugin.h)
    target_link_libraries(brutalpress_shared INTERFACE brutalpress_engine)
    target_include_directories(brutalpress_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(APPLE AND BRUTALPRESS_ADHOC_SIGN)
        find_program(BRUTALPRESS_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET brutalpress_vst3_plugin POST_BUILD
            COMMAND "${BRUTALPRESS_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:brutalpress_vst3_plugin>"
            COMMENT "Ad-hoc signing BrutalPress VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET brutalpress_standalone_plugin POST_BUILD
            COMMAND "${BRUTALPRESS_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:brutalpress_standalone_plugin>"
            COMMENT "Ad-hoc signing BrutalPress standalone app for local launch"
            VERBATIM)
    endif()
endif()
