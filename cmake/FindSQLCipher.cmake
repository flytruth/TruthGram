# =============================================================================
# FindSQLCipher.cmake - locate SQLCipher 4.x and expose SQLCipher::SQLCipher
# =============================================================================
# SQLCipher is a drop-in replacement for SQLite with AES-256 encryption. It
# does not ship a CMake config, so we emulate find_package semantics here.
#
# Strategy:
#   1. pkg-config module "sqlcipher" (Arch: sqlcipher pkg; Debian/Ubuntu:
#      libsqlcipher-dev) -> gives the exact include dir + library + version.
#   2. Fall back to find_path / find_library. Crucially, PATH_SUFFIXES is
#      `sqlcipher` so that the include dir resolves to the directory that
#      *directly contains* sqlite3.h (e.g. /usr/include/sqlcipher on Arch and
#      Debian), and `#include <sqlite3.h>` in truthgram_database.cpp picks up
#      the SQLCipher copy, NOT the plain-system SQLite.
#
# The two SQLCipher-specific preprocessor defines travel with the target as
# INTERFACE_COMPILE_DEFINITIONS. They are written as a CMake list
# (semicolon-separated), NOT a single space-separated string, so CMake emits
# `-DSQLITE_HAS_CODEC -DSQLITE_THREADSAFE=1` on the command line. The previous
# form (`"SQLITE_HAS_CODEC SQLITE_THREADSAFE=1"`) made CMake emit
# `-DSQLITE_HAS_CODEC SQLITE_THREADSAFE=1`, where the bare
# `SQLITE_THREADSAFE=1` token was mis-read as a source file. See the comment
# below for the exact rule.

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(PC_SQLCIPHER QUIET sqlcipher)
endif()

if(PC_SQLCIPHER_FOUND)
    set(SQLCipher_VERSION "${PC_SQLCIPHER_VERSION}")
    find_path(SQLCipher_INCLUDE_DIR
        NAMES sqlite3.h
        HINTS ${PC_SQLCIPHER_INCLUDE_DIRS}
        PATH_SUFFIXES sqlcipher
        DOC "Path to SQLCipher's sqlite3.h (directory containing it)"
    )
    find_library(SQLCipher_LIBRARY
        NAMES ${PC_SQLCIPHER_LIBRARIES} sqlcipher
        HINTS ${PC_SQLCIPHER_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64
        DOC "Path to libsqlcipher"
    )
else()
    find_path(SQLCipher_INCLUDE_DIR
        NAMES sqlite3.h
        PATH_SUFFIXES sqlcipher
        DOC "Path to SQLCipher's sqlite3.h (directory containing it)"
    )
    find_library(SQLCipher_LIBRARY
        NAMES sqlcipher
        PATH_SUFFIXES lib lib64
        DOC "Path to libsqlcipher"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLCipher
    REQUIRED_VARS SQLCipher_LIBRARY SQLCipher_INCLUDE_DIR
    VERSION_VAR SQLCipher_VERSION)

# Create the imported target exactly once so duplicate-include is safe.
if(SQLCipher_FOUND AND NOT TARGET SQLCipher::SQLCipher)
    add_library(SQLCipher::SQLCipher UNKNOWN IMPORTED)
    set_target_properties(SQLCipher::SQLCipher PROPERTIES
        IMPORTED_LOCATION "${SQLCipher_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLCipher_INCLUDE_DIR}"
        # Semicolon-separated CMake list -> CMake emits one -D per entry:
        #   -DSQLITE_HAS_CODEC -DSQLITE_THREADSAFE=1
        # Never write these space-separated in a single string, or the bare
        # `SQLITE_THREADSAFE=1` token is parsed by the compiler as a source
        # file and the build breaks with "file not found".
        INTERFACE_COMPILE_DEFINITIONS "SQLITE_HAS_CODEC;SQLITE_THREADSAFE=1")
endif()

mark_as_advanced(SQLCipher_INCLUDE_DIR SQLCipher_LIBRARY)
