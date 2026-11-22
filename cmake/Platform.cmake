# =============================================================================
# Platform.cmake - OS detection and platform-specific helpers for TruthGram
# =============================================================================
# Sets up the canonical platform variables TruthGram relies on. Avoids the
# common pitfall of testing only WIN32/_WIN32 by also covering QNX, EMSCRIPTEN
# etc. — but TruthGram only targets Windows, Linux, and macOS today.

set(TRUTHGRAM_PLATFORM "")

if(WIN32)
    set(TRUTHGRAM_PLATFORM "windows")
    add_compile_definitions(TRUTHGRAM_PLATFORM_WINDOWS=1)
    # Windows UTC/Local time helpers used by cloud_sync timestamps.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    set(TRUTHGRAM_PLATFORM "macos")
    add_compile_definitions(TRUTHGRAM_PLATFORM_MACOS=1)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS min version")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
else()
    set(TRUTHGRAM_PLATFORM "linux")
    add_compile_definitions(TRUTHGRAM_PLATFORM_LINUX=1)
endif()

# Output directory layout: everything lands in build/bin so packaging scripts
# can find a single staging root.
function(truthgram_set_output_dirs target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        PDB_OUTPUT_DIRECTORY    "${CMAKE_BINARY_DIR}/bin")
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin"
            LIBRARY_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/lib"
            ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/lib")
    endforeach()
endfunction()

# Per-target platform suffix for shared libraries (plugins).
function(truthgram_plugin_output_name target base_name)
    if(WIN32)
        set_target_properties(${target} PROPERTIES
            PREFIX "" OUTPUT_NAME "${base_name}" SUFFIX ".dll")
    elseif(APPLE)
        set_target_properties(${target} PROPERTIES
            PREFIX "lib" OUTPUT_NAME "${base_name}" SUFFIX ".dylib")
    else()
        set_target_properties(${target} PROPERTIES
            PREFIX "lib" OUTPUT_NAME "${base_name}" SUFFIX ".so")
    endif()
endfunction()
