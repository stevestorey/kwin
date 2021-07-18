#.rst:
# Findlibxcvt
# -------
#
# Try to find libxcvt on a Unix system.
#
# This will define the following variables:
#
# ``libxcvt_FOUND``
#     True if (the requested version of) libxcvt is available
# ``libxcvt_VERSION``
#     The version of libxcvt
# ``libxcvt_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``libxcvt::libxcvt``
#     target
# ``libxcvt_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``libxcvt_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``libxcvt_FOUND`` is TRUE, it will also define the following imported target:
#
# ``libxcvt::libxcvt``
#     The libxcvt library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# SPDX-FileCopyrightText: 2014 Alex Merry <alex.merry@kde.org>
# SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
# SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by Findlibxcvt.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use Findlibxcvt.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_libxcvt QUIET libxcvt)

    set(libxcvt_DEFINITIONS ${PKG_libxcvt_CFLAGS_OTHER})
    set(libxcvt_VERSION ${PKG_libxcvt_VERSION})

    find_path(libxcvt_INCLUDE_DIR
        NAMES
            libxcvt/libxcvt.h
        HINTS
            ${PKG_libxcvt_INCLUDE_DIRS}
    )
    find_library(libxcvt_LIBRARY
        NAMES
            xcvt
        HINTS
            ${PKG_libxcvt_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libxcvt
        FOUND_VAR
            libxcvt_FOUND
        REQUIRED_VARS
            libxcvt_LIBRARY
            libxcvt_INCLUDE_DIR
        VERSION_VAR
            libxcvt_VERSION
    )

    if(libxcvt_FOUND AND NOT TARGET libxcvt::libxcvt)
        add_library(libxcvt::libxcvt UNKNOWN IMPORTED)
        set_target_properties(libxcvt::libxcvt PROPERTIES
            IMPORTED_LOCATION "${libxcvt_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libxcvt_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libxcvt_INCLUDE_DIR}"
            INTERFACE_INCLUDE_DIRECTORIES "${libxcvt_INCLUDE_DIR}/libxcvt"
        )
    endif()

    mark_as_advanced(libxcvt_LIBRARY libxcvt_INCLUDE_DIR)

    # compatibility variables
    set(libxcvt_LIBRARIES ${libxcvt_LIBRARY})
    set(libxcvt_INCLUDE_DIRS ${libxcvt_INCLUDE_DIR})
    set(libxcvt_VERSION_STRING ${libxcvt_VERSION})

else()
    message(STATUS "Findlibxcvt.cmake cannot find libxcvt on Windows systems.")
    set(libxcvt_FOUND FALSE)
endif()

include(FeatureSummary)
set_package_properties(libxcvt PROPERTIES
    URL "https://gitlab.freedesktop.org/xorg/lib/libxcvt"
    DESCRIPTION "libxcvt is a library providing a standalone version of the X server implementation of the VESA CVT standard timing modelines generator."
)
