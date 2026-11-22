# =============================================================================
# FindFFMPEG.cmake - locate FFmpeg libav* and expose FFmpeg::<lib> targets.
# =============================================================================
# Works on Arch Linux (ffmpeg core package) and Debian/Ubuntu (libavcodec-dev
# etc.) by trying pkg-config first, then falling back to find_path/find_library.
# Each component (avcodec, avformat, avutil, ...) becomes an imported target
# named FFmpeg::<component> with include dirs and link flags attached.
#
# Usage:
#   find_package(FFMPEG [REQUIRED] [COMPONENTS avcodec avformat avutil ...])
#
# This file is safe to include more than once: every add_library() is guarded
# by `if(NOT TARGET FFmpeg::FFMPEG)`.

find_package(PkgConfig QUIET)

# The set of FFmpeg libraries we know how to find. The caller passes a subset
# via COMPONENTS; we intersect with this list.
set(_FFMPEG_KNOWN_COMPONENTS
    avcodec avdevice avfilter avformat avutil swresample swscale postproc)

if(FFMPEG_FIND_COMPONENTS)
    set(_FFMPEG_REQUESTED ${FFMPEG_FIND_COMPONENTS})
else()
    set(_FFMPEG_REQUESTED ${_FFMPEG_KNOWN_COMPONENTS})
endif()

# Resolve every requested component via pkg-config, then fall back to a plain
# find_path/find_library pair. We accumulate the include dirs and libraries
# into FFMPEG_INCLUDE_DIRS / FFMPEG_LIBRARIES for the legacy variables, and
# also build one imported target per component plus an umbrella FFmpeg::FFMPEG.
set(FFMPEG_INCLUDE_DIRS "")
set(FFMPEG_LIBRARIES "")
set(_FFMPEG_ALL_FOUND TRUE)

foreach(_comp IN LISTS _FFMPEG_REQUESTED)
    # pkg-config module name is "lib<comp>" (e.g. libavcodec).
    set(_pc_module "lib${_comp}")
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_${_comp} QUIET ${_pc_module})
    endif()

    if(PC_${_comp}_FOUND)
        set(_inc_dirs "${PC_${_comp}_INCLUDE_DIRS}")
        set(_lib_dirs "${PC_${_comp}_LIBRARY_DIRS}")
        set(_libs      "${PC_${_comp}_LIBRARIES}")
        set(_version   "${PC_${_comp}_VERSION}")
        # find_path/find_library over the pkg-config hints so we get an
        # absolute file path for IMPORTED_LOCATION.
        find_path(${_comp}_INCLUDE_DIR
            NAMES lib${_comp}/avcodec.h
                  lib${_comp}/avformat.h
                  lib${_comp}/avutil.h
                  lib${_comp}/swresample.h
                  lib${_comp}/swscale.h
            HINTS ${_inc_dirs}
            PATH_SUFFIXES include
            DOC "Path to lib${_comp} headers"
        )
    else()
        find_path(${_comp}_INCLUDE_DIR
            NAMES lib${_comp}/avcodec.h
                  lib${_comp}/avformat.h
                  lib${_comp}/avutil.h
                  lib${_comp}/swresample.h
                  lib${_comp}/swscale.h
            PATH_SUFFIXES include
            DOC "Path to lib${_comp} headers"
        )
        find_library(${_comp}_LIBRARY
            NAMES ${_comp}
            PATH_SUFFIXES lib lib64
            DOC "Path to lib${_comp}"
        )
        set(_libs "${_comp}")
        set(_version "")
    endif()

    # The library file itself: prefer the find_library result, else use the
    # first pkg-config library entry resolved against the lib dirs.
    if(NOT ${_comp}_LIBRARY AND PC_${_comp}_FOUND)
        foreach(_l IN LISTS PC_${_comp}_LIBRARIES)
            find_library(_resolved_${_comp} NAMES ${_l} HINTS ${PC_${_comp}_LIBRARY_DIRS})
            if(_resolved_${_comp})
                set(${_comp}_LIBRARY "${_resolved_${_comp}}")
                break()
            endif()
        endforeach()
    endif()

    if(${_comp}_INCLUDE_DIR AND ${_comp}_LIBRARY)
        set(FFMPEG_${_comp}_FOUND TRUE)
        if(FFMPEG_INCLUDE_DIRS)
            string(APPEND FFMPEG_INCLUDE_DIRS " ${${_comp}_INCLUDE_DIR}")
        else()
            set(FFMPEG_INCLUDE_DIRS "${${_comp}_INCLUDE_DIR}")
        endif()
        if(FFMPEG_LIBRARIES)
            string(APPEND FFMPEG_LIBRARIES " ${${_comp}_LIBRARY}")
        else()
            set(FFMPEG_LIBRARIES "${${_comp}_LIBRARY}")
        endif()

        if(NOT TARGET FFmpeg::${_comp})
            add_library(FFmpeg::${_comp} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${_comp} PROPERTIES
                IMPORTED_LOCATION "${${_comp}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${${_comp}_INCLUDE_DIR}")
        endif()
        if(_version)
            set_target_properties(FFmpeg::${_comp} PROPERTIES
                INTERFACE_VERSION "${_version}")
        endif()
    else()
        # Component not found. We only set FFMPEG_<comp>_FOUND=FALSE here and
        # let find_package_handle_standard_args(HANDLE_COMPONENTS) decide
        # whether that is fatal: it FATAL_ERRORs only when the caller passed
        # REQUIRED (or required this specific component). A plain
        # find_package(FFMPEG COMPONENTS ...) with no deps installed must
        # simply report FFMPEG_FOUND=FALSE and continue.
        set(FFMPEG_${_comp}_FOUND FALSE)
        set(_FFMPEG_ALL_FOUND FALSE)
    endif()
endforeach()

# Umbrella target linking every found component.
if(_FFMPEG_ALL_FOUND AND FFMPEG_LIBRARIES AND NOT TARGET FFmpeg::FFMPEG)
    add_library(FFmpeg::FFMPEG INTERFACE IMPORTED)
    set(_deps "")
    foreach(_comp IN LISTS _FFMPEG_REQUESTED)
        if(TARGET FFmpeg::${_comp})
            list(APPEND _deps FFmpeg::${_comp})
        endif()
    endforeach()
    set_target_properties(FFmpeg::FFMPEG PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_deps}")
endif()

include(FindPackageHandleStandardArgs)
# Handle REQUIRED / QUIET / version. We only validate the per-component
# results so the standard args macro reports a clean reason on failure.
find_package_handle_standard_args(FFMPEG
    REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS
    HANDLE_COMPONENTS)

mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES)
