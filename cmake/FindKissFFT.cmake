# =============================================================================
# FindKissFFT.cmake - optionally locate Kiss FFT for the Mini Player FFT bars.
# =============================================================================
# Optional dependency. When found, sets TRUTHGRAM_HAVE_KISSFFT=1 (consumed by
# mini_player.h/.cpp to use the real kiss_fftr API). When not found, the Mini
# Player still builds and runs — the waveform bars simply render zero-height
# (the player controls, speed slider, pitch lock all work regardless).
#
# Kiss FFT ships several flavours (float / double / int / fast). We prefer the
# float flavour since the Mini Player feeds float samples. pkg-config module
# names vary by distro (Arch: kissfft-float; Debian: kissfft-float), so we try
# a list and fall back to find_path/find_library.

find_package(PkgConfig QUIET)

set(_KISSFFT_PC_MODULES kissfft-float kissfft_float kissfft)
set(_KISSFFT_PC_FOUND FALSE)
foreach(_pc IN LISTS _KISSFFT_PC_MODULES)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_KISSFFT QUIET ${_pc})
    endif()
    if(PC_KISSFFT_FOUND)
        set(_KISSFFT_PC_FOUND TRUE)
        break()
    endif()
endforeach()

if(_KISSFFT_PC_FOUND)
    find_path(KissFFT_INCLUDE_DIR
        NAMES kiss_fft.h kiss_fftr.h
        HINTS ${PC_KISSFFT_INCLUDE_DIRS}
        PATH_SUFFIXES include
        DOC "Path to Kiss FFT headers"
    )
    find_library(KissFFT_LIBRARY
        NAMES ${PC_KISSFFT_LIBRARIES} kissfft_float kissfft-float kissfft
        HINTS ${PC_KISSFFT_LIBRARY_DIRS}
        PATH_SUFFIXES lib lib64
        DOC "Path to libkissfft"
    )
else()
    find_path(KissFFT_INCLUDE_DIR
        NAMES kiss_fft.h kiss_fftr.h
        PATH_SUFFIXES include kissfft
        DOC "Path to Kiss FFT headers"
    )
    find_library(KissFFT_LIBRARY
        NAMES kissfft_float kissfft-float kissfft
        PATH_SUFFIXES lib lib64
        DOC "Path to libkissfft"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KissFFT
    REQUIRED_VARS KissFFT_LIBRARY KissFFT_INCLUDE_DIR)

if(KissFFT_FOUND AND NOT TARGET KissFFT::KissFFT)
    add_library(KissFFT::KissFFT UNKNOWN IMPORTED)
    set_target_properties(KissFFT::KissFFT PROPERTIES
        IMPORTED_LOCATION "${KissFFT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${KissFFT_INCLUDE_DIR}")
endif()

mark_as_advanced(KissFFT_INCLUDE_DIR KissFFT_LIBRARY)
