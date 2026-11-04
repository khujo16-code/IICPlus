cmake_minimum_required(VERSION 3.22)
project(IICPlusNAM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")
set(NAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/NeuralAmpModelerCore")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Run scripts/setup.ps1 first.")
endif()
if(NOT EXISTS "${NAM_DIR}/NAM/get_dsp.cpp")
    message(FATAL_ERROR "NeuralAmpModelerCore not found or incomplete. Run scripts/setup.ps1 first.")
endif()
if(NOT EXISTS "${NAM_DIR}/Dependencies/eigen/Eigen/Core")
    message(FATAL_ERROR "Eigen submodule missing. Run scripts/setup.ps1 with recursive clone.")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

# Compile NAM Core sources directly into the executable. Its architecture parser
# registration is performed by static initializers in individual translation
# units; putting these in a static library can allow the linker to omit them.
file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
    "${NAM_DIR}/NAM/*.cpp"
    "${NAM_DIR}/NAM/*.c"
    "${NAM_DIR}/NAM/*/*.cpp"
)
list(APPEND NAM_SOURCES "${NAM_DIR}/Dependencies/AudioDSPTools/dsp/wav.cpp")

juce_add_gui_app(IICPlusNAM
    PRODUCT_NAME "IIC+ NAM"
    COMPANY_NAME "IICPlusNAM"
    BUNDLE_ID "com.iicplusnam.standalone"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
)

target_sources(IICPlusNAM PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/AudioEngine.h
    Source/AudioEngine.cpp
    ${NAM_SOURCES}
)

juce_generate_juce_header(IICPlusNAM)

target_include_directories(IICPlusNAM PRIVATE
    "${NAM_DIR}"
    "${NAM_DIR}/NAM"
    "${NAM_DIR}/Dependencies/eigen"
    "${NAM_DIR}/Dependencies/nlohmann"
    "${NAM_DIR}/Dependencies/AudioDSPTools/dsp"
)

target_compile_definitions(IICPlusNAM PRIVATE
    NAM_SAMPLE_FLOAT DSP_SAMPLE_FLOAT
    JUCE_WEB_BROWSER=0
    JUCE_ASIO=1
    JUCE_ASIO_USE_EXTERNAL_SDK=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:IICPlusNAM,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    NOMINMAX
)

target_link_libraries(IICPlusNAM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(IICPlusNAM PRIVATE /W4 /permissive-)
endif()

# Ship the fixed NAM profiles and cabinet IR beside the executable.
add_custom_command(TARGET IICPlusNAM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/profiles"
        "$<TARGET_FILE_DIR:IICPlusNAM>/profiles"
    COMMENT "Copying fixed IIC+ NAM profiles and cabinet IR"
)


# Ship visual assets beside the executable (used by the DRIVE knob prototype).
add_custom_command(TARGET IICPlusNAM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:IICPlusNAM>/assets"
    COMMENT "Copying IIC+ visual assets"
)
