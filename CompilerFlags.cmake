# =============================================================================
# CompilerFlags.cmake - C++20 compiler flags shared across TruthGram modules
# =============================================================================
# TruthGram must compile cleanly under -Wall -Wextra on GCC/Clang and /W4 /WX
# on MSVC. ASAN and LTO are opt-in.

function(truthgram_apply_config_defines target)
    target_compile_definitions(${target} PUBLIC
        TRUTHGRAM_VERSION="${PROJECT_VERSION}"
        TRUTHGRAM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        TRUTHGRAM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        TRUTHGRAM_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )
    if(TRUTHGRAM_ENABLE_IOS_UI)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_IOS_UI=1)
    endif()
    if(TRUTHGRAM_ENABLE_MINI_PLAYER)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_MINI_PLAYER=1)
    endif()
    if(TRUTHGRAM_ENABLE_PLUGINS)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_PLUGINS=1)
    endif()
    if(TRUTHGRAM_ENABLE_LUA_PLUGINS)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_LUA_PLUGINS=1)
    endif()
    if(TRUTHGRAM_ENABLE_PYTHON_PLUGINS)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_PYTHON_PLUGINS=1)
    endif()
    if(TRUTHGRAM_ENABLE_CLOUD_SYNC)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_CLOUD_SYNC=1)
    endif()
    if(TRUTHGRAM_ENABLE_VOICE_CHANGER)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_VOICE_CHANGER=1)
    endif()
    if(TRUTHGRAM_ENABLE_GHOST)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_GHOST=1)
    endif()
    if(TRUTHGRAM_ENABLE_PHISHING)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_PHISHING=1)
    endif()
    if(TRUTHGRAM_ENABLE_WEBAPP_AUDIT)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_WEBAPP_AUDIT=1)
    endif()
    if(TRUTHGRAM_ENABLE_OSINT)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_OSINT=1)
    endif()
    if(TRUTHGRAM_ENABLE_SCREEN_SHIELD)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_SCREEN_SHIELD=1)
    endif()
    if(TRUTHGRAM_ENABLE_CHESS)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_CHESS=1)
    endif()
    if(TRUTHGRAM_ENABLE_BOT_STUDIO)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_BOT_STUDIO=1)
    endif()
    if(TRUTHGRAM_ENABLE_TERMUX_BRIDGE)
        target_compile_definitions(${target} PUBLIC TRUTHGRAM_ENABLE_TERMUX_BRIDGE=1)
    endif()
endfunction()

if(MSVC)
    add_compile_options(
        /std:c++20
        /permissive-
        /Zc:__cxaApiExport
        /Zc:preprocessor
        /Zc:referenceBinding
        /Zc:rvalueCast
        /utf-8
        /W4
        /w14242   # conversion from 'int' to 'char', possible loss of data
        /w14263   # member function does not override any base class virtual
        /EHsc
        /bigobj
    )
    # Disable the noisy telemetry-style warnings that tdesktop suppresses.
    add_compile_definitions(_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-unknown-pragmas
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_compile_options(-Wno-implicit-fallthrough -Wno-range-loop-analysis)
    endif()
endif()

# LTO in Release.
if(TRUTHGRAM_ENABLE_LTO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif()

# ASAN in Debug, opt-in.
if(TRUTHGRAM_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
    else()
        add_compile_options(/fsanitize=address)
    endif()
endif()

# Position-independent code (required for the plugin SDK shared libs).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
