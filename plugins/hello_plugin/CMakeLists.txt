# =============================================================================
# TruthGram hello_plugin - example native plugin
# =============================================================================
# This CMakeLists builds a single shared library (libhello_plugin.so /
# hello_plugin.dll / libhello_plugin.dylib) that the TruthGram Plugin Manager
# loads at runtime. The plugin links Qt Widgets and includes only the public
# plugin_sdk.h header; it does NOT link the tg_truthgram target itself.
cmake_minimum_required(VERSION 3.25)

project(hello_plugin LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# The TruthGram SDK header lives at Telegram/SourceFiles/truthgram/plugins/.
# Point TRUTHGRAM_SDK_DIR at a TruthGram checkout (or its installed include dir).
set(TRUTHGRAM_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Telegram/SourceFiles/truthgram"
    CACHE PATH "Path to the TruthGram plugin SDK headers")

add_library(hello_plugin SHARED hello_plugin.cpp)

target_include_directories(hello_plugin PRIVATE
    "${TRUTHGRAM_SDK_DIR}"
    "${TRUTHGRAM_SDK_DIR}/plugins")

target_link_libraries(hello_plugin PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets)

# Strip the lib prefix and use a stable suffix so the manifest "entry"
# resolves correctly on every platform.
set_target_properties(hello_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hello_plugin")

if(WIN32)
    set_target_properties(hello_plugin PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(hello_plugin PROPERTIES
        PREFIX "lib" SUFFIX ".dylib")
else()
    set_target_properties(hello_plugin PROPERTIES
        PREFIX "lib" SUFFIX ".so")
endif()

# Install step: copy the library + manifest into the user plugin dir so the
# Plugin Manager picks it up automatically on next launch.
install(TARGETS hello_plugin
    LIBRARY DESTINATION "$ENV{HOME}/.local/share/TruthGram/plugins/hello_plugin"
    RUNTIME DESTINATION "$ENV{HOME}/.local/share/TruthGram/plugins/hello_plugin")
install(FILES manifest.json
    DESTINATION "$ENV{HOME}/.local/share/TruthGram/plugins/hello_plugin")
