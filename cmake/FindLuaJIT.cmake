# =============================================================================
# FindLuaJIT.cmake - locate LuaJIT and expose the LuaJIT::LuaJIT target.
# =============================================================================
# Works on Arch Linux (luajit package, headers under /usr/include/luajit-2.1)
# and Debian/Ubuntu (libluajit-5.1-dev, headers under /usr/include/luajit-2.1).
#
# Strategy:
#   1. pkg-config (module "luajit" on Arch, "luajit" / "luajit-5.1" elsewhere)
#      -> gives the exact include dir + library + version.
#   2. Fall back to find_path (with the luajit-2.1 / luajit suffix so that
#      `#include <lua.h>` resolves to the LuaJIT copy, not plain Lua) and
#      find_library for the .so/.a.
#
# The imported target LuaJIT::LuaJIT is created exactly once — the
# `if(NOT TARGET LuaJIT::LuaJIT)` guard means re-running find_package (e.g.
# when TruthGram is embedded in a larger build) never raises a duplicate
# target error.

find_package(PkgConfig QUIET)

# Try the known pkg-config module names in order.
set(_LUAJIT_PC_MODULES luajit luajit-5.1 lua51)
set(_LUAJIT_PC_FOUND FALSE)
foreach(_pc IN LISTS _LUAJIT_PC_MODULES)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_LUAJIT QUIET ${_pc})
    endif()
    if(PC_LUAJIT_FOUND)
        set(_LUAJIT_PC_FOUND TRUE)
        set(_LUAJIT_PC_MODULE "${_pc}")
        break()
    endif()
endforeach()

if(_LUAJIT_PC_FOUND)
    set(LUAJIT_VERSION "${PC_LUAJIT_VERSION}")
    # find_path/find_library over the pkg-config hints so we get absolute
    # paths for the imported target (pkg-config alone gives compiler flags).
    find_path(LUAJIT_INCLUDE_DIR
        NAMES lua.h lualib.h lauxlib.h
        HINTS ${PC_LUAJIT_INCLUDE_DIRS}
        PATH_SUFFIXES luajit-2.1 luajit
        DOC "Path to LuaJIT headers (directory containing lua.h)"
    )
    # Resolve the actual library file (PC_LUAJIT_LIBRARIES is a name list).
    find_library(LUAJIT_LIBRARY
        NAMES ${PC_LUAJIT_LIBRARIES} luajit luajit-5.1
        HINTS ${PC_LUAJIT_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64
        DOC "Path to libluajit"
    )
else()
    # No pkg-config — fall back to a plain find. The PATH_SUFFIXES are what
    # make `#include <lua.h>` find the LuaJIT copy on Arch/Debian, where the
    # headers live in /usr/include/luajit-2.1/ rather than /usr/include/.
    find_path(LUAJIT_INCLUDE_DIR
        NAMES lua.h lualib.h lauxlib.h
        PATH_SUFFIXES luajit-2.1 luajit include/luajit-2.1 include/luajit
        DOC "Path to LuaJIT headers (directory containing lua.h)"
    )
    find_library(LUAJIT_LIBRARY
        NAMES luajit luajit-5.1
        PATH_SUFFIXES lib lib64
        DOC "Path to libluajit"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LuaJIT
    REQUIRED_VARS LUAJIT_LIBRARY LUAJIT_INCLUDE_DIR
    VERSION_VAR LUAJIT_VERSION)

# Create the imported target exactly once. Downstream code does
#   target_link_libraries(my_target PRIVATE LuaJIT::LuaJIT)
# and gets both the include dir (so `#include <lua.h>` resolves to LuaJIT) and
# the link flag.
if(LUAJIT_FOUND AND NOT TARGET LuaJIT::LuaJIT)
    add_library(LuaJIT::LuaJIT UNKNOWN IMPORTED)
    set_target_properties(LuaJIT::LuaJIT PROPERTIES
        IMPORTED_LOCATION "${LUAJIT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "m;dl"
        # LuaJIT needs -lm and -ldl on Linux. These are no-ops on Windows.
    )
endif()

mark_as_advanced(LUAJIT_INCLUDE_DIR LUAJIT_LIBRARY)
