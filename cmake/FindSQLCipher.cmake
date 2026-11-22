# =============================================================================
# FindSQLCipher.cmake - locate SQLCipher 4.x and expose SQLCipher::SQLCipher
# =============================================================================
# SQLCipher is a drop-in replacement for SQLite with AES-256 encryption. It
# does not ship a CMake config, so we emulate find_package semantics here.

find_path(SQLCipher_INCLUDE_DIR
    NAMES sqlcipher/sqlite3.h sqlite3.h
    PATH_SUFFIXES include sqlcipher
)
find_library(SQLCipher_LIBRARY
    NAMES sqlcipher sqlite3
    PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLCipher
    REQUIRED_VARS SQLCipher_LIBRARY SQLCipher_INCLUDE_DIR)

if(SQLCipher_FOUND AND NOT TARGET SQLCipher::SQLCipher)
    add_library(SQLCipher::SQLCipher UNKNOWN IMPORTED)
    set_target_properties(SQLCipher::SQLCipher PROPERTIES
        IMPORTED_LOCATION "${SQLCipher_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLCipher_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "SQLITE_HAS_CODEC SQLITE_THREADSAFE=1")
endif()

mark_as_advanced(SQLCipher_INCLUDE_DIR SQLCipher_LIBRARY)
