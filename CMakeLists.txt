cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versione minima di macOS")

project(Ipnosy VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # runtime statico: niente DLL di Visual C++ da installare
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------- JUCE
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(juce)

# ---------------------------------------------------- NeuralAmpModelerCore
# Scaricato senza il suo CMakeLists: compiliamo solo la cartella NAM/
FetchContent_Declare(namcore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        v0.6.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake_)
FetchContent_MakeAvailable(namcore)

# Eigen: dal sottomodulo di NAM, altrimenti scaricato a parte
file(GLOB_RECURSE _eigen_core "${namcore_SOURCE_DIR}/Dependencies/*/Eigen/Core")
if(_eigen_core)
    list(GET _eigen_core 0 _eigen_first)
    get_filename_component(_eigen_dir "${_eigen_first}" DIRECTORY)
    get_filename_component(EIGEN_INCLUDE_DIR "${_eigen_dir}" DIRECTORY)
else()
    FetchContent_Declare(eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _no_cmake_)
    FetchContent_MakeAvailable(eigen)
    set(EIGEN_INCLUDE_DIR "${eigen_SOURCE_DIR}")
endif()
message(STATUS "Eigen: ${EIGEN_INCLUDE_DIR}")

# nlohmann json: dalla cartella Dependencies di NAM, altrimenti scaricato a parte
file(GLOB_RECURSE _json_hpp "${namcore_SOURCE_DIR}/Dependencies/*json.hpp")
set(JSON_INCLUDE_DIRS "")
if(_json_hpp)
    foreach(_j ${_json_hpp})
        get_filename_component(_jd "${_j}" DIRECTORY)
        get_filename_component(_jp "${_jd}" DIRECTORY)
        list(APPEND JSON_INCLUDE_DIRS "${_jd}" "${_jp}")
    endforeach()
else()
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  _no_cmake_)
    FetchContent_MakeAvailable(nlohmann_json)
    list(APPEND JSON_INCLUDE_DIRS
        "${nlohmann_json_SOURCE_DIR}/single_include"
        "${nlohmann_json_SOURCE_DIR}/single_include/nlohmann")
endif()
list(REMOVE_DUPLICATES JSON_INCLUDE_DIRS)
message(STATUS "json: ${JSON_INCLUDE_DIRS}")

file(GLOB_RECURSE NAM_SOURCES CONFIGURE_DEPENDS "${namcore_SOURCE_DIR}/NAM/*.cpp")

# Libreria OBJECT: gli oggetti finiscono direttamente nel binario del plugin,
# cosi' le registrazioni statiche delle architetture (WaveNet, SlimmableContainer...)
# non vengono scartate dal linker.
add_library(nam_core OBJECT ${NAM_SOURCES})
target_include_directories(nam_core PRIVATE
    "${namcore_SOURCE_DIR}"
    "${namcore_SOURCE_DIR}/NAM"
    "${EIGEN_INCLUDE_DIR}"
    ${JSON_INCLUDE_DIRS})
target_compile_features(nam_core PRIVATE cxx_std_20)
if(MSVC)
    target_compile_definitions(nam_core PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(nam_core PRIVATE /bigobj /fp:fast $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(nam_core PRIVATE -O3 -w)
endif()

# ---------------------------------------------------------------- Plugin
juce_add_plugin(Ipnosy
    COMPANY_NAME             "Meteora Plugins"
    COMPANY_COPYRIGHT        "Copyright (c) 2026 Meteora Plugins. Tutti i diritti riservati."
    BUNDLE_ID                com.meteoraplugins.ipnosy
    PLUGIN_MANUFACTURER_CODE Mtra
    PLUGIN_CODE              Ipns
    FORMATS                  VST3
    PRODUCT_NAME             "Ipnosy"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Distortion)

juce_add_binary_data(IpnosyData SOURCES
    Resources/models/ag_0900.nam
    Resources/models/ag_1200.nam
    Resources/models/ag_max.nam
    Resources/gui/background.jpg
    Resources/gui/logo.png
    Resources/gui/blend.png
    Resources/gui/led_on.png
    Resources/gui/tube_fil.png
    Resources/gui/tube_full.png
    Resources/gui/leaf.png)

target_sources(Ipnosy PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TubeEngine.h
    Source/Tube2Stage.h
    Source/LinearPhaseOversampler.h
    Source/AutoGain.h
    Source/Resampler.h)

target_include_directories(Ipnosy PRIVATE
    Source
    "${namcore_SOURCE_DIR}"
    "${EIGEN_INCLUDE_DIR}"
    ${JSON_INCLUDE_DIRS})

target_compile_definitions(Ipnosy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_definitions(Ipnosy PUBLIC NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(Ipnosy PRIVATE /bigobj)
endif()

target_link_libraries(Ipnosy
    PRIVATE
        IpnosyData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)

# gli oggetti di NAM vanno nel target finale VST3
target_link_libraries(Ipnosy_VST3 PRIVATE nam_core)
