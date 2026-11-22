# =============================================================================
# TruthGramConfig.cmake - export TruthGram::tg_truthgram for downstream
# consumers (packagers, plugin toolchains that link the host ABI).
# =============================================================================
# This file is installed alongside the archive; it lets a downstream CMake
# project do:
#
#   find_package(TruthGram 1.0 REQUIRED)
#   target_link_libraries(my_plugin PRIVATE TruthGram::Host)

include("${CMAKE_CURRENT_LIST_DIR}/TruthGramTargets.cmake")

set(TRUTHGRAM_VERSION "@PROJECT_VERSION@")
set(TRUTHGRAM_VERSION_MAJOR @PROJECT_VERSION_MAJOR@)
set(TRUTHGRAM_VERSION_MINOR @PROJECT_VERSION_MINOR@)
set(TRUTHGRAM_VERSION_PATCH @PROJECT_VERSION_PATCH@)

set(TRUTHGRAM_HAVE_IOS_UI          @TRUTHGRAM_ENABLE_IOS_UI@)
set(TRUTHGRAM_HAVE_PLUGINS         @TRUTHGRAM_ENABLE_PLUGINS@)
set(TRUTHGRAM_HAVE_CLOUD_SYNC      @TRUTHGRAM_ENABLE_CLOUD_SYNC@)
set(TRUTHGRAM_HAVE_VOICE_CHANGER   @TRUTHGRAM_ENABLE_VOICE_CHANGER@)
set(TRUTHGRAM_HAVE_GHOST           @TRUTHGRAM_ENABLE_GHOST@)
set(TRUTHGRAM_HAVE_PHISHING        @TRUTHGRAM_ENABLE_PHISHING@)
set(TRUTHGRAM_HAVE_WEBAPP_AUDIT    @TRUTHGRAM_ENABLE_WEBAPP_AUDIT@)
set(TRUTHGRAM_HAVE_OSINT           @TRUTHGRAM_ENABLE_OSINT@)
set(TRUTHGRAM_HAVE_SCREEN_SHIELD   @TRUTHGRAM_ENABLE_SCREEN_SHIELD@)
set(TRUTHGRAM_HAVE_CHESS           @TRUTHGRAM_ENABLE_CHESS@)
set(TRUTHGRAM_HAVE_BOT_STUDIO      @TRUTHGRAM_ENABLE_BOT_STUDIO@)
set(TRUTHGRAM_HAVE_TERMUX_BRIDGE   @TRUTHGRAM_ENABLE_TERMUX_BRIDGE@)

# The public plugin host ABI is a single header-only interface.
add_library(TruthGram::Host INTERFACE IMPORTED)
set_target_properties(TruthGram::Host PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/../../../include/truthgram/plugins")

check_required_components(TruthGram)
