# - Try to find Cocaine
# Once done this will define:
#  LIBCOCAINE_FOUND         - system has Cocaine
#  LIBCOCAINE_INCLUDE_DIRS  - the Cocaine include directories
#  LIBCOCAINE_LIBRARIES     - the libraries needed to use Cocaine
#  LIBCOCAINE_DEFINITIONS   - compiler switches required for using Cocaine

find_package(PkgConfig)
pkg_check_modules(PC_LIBCOCAINE QUIET libcocaine-core)
set(LIBCOCAINE_DEFINITIONS ${PC_LIBCOCAINE_CFLAGS_OTHER})

find_path(
    LIBCOCAINE_INCLUDE_DIR cocaine/config.hpp
    HINTS ${PC_LIBCOCAINE_INCLUDEDIR} ${PC_LIBCOCAINE_INCLUDE_DIRS}
    PATH_SUFFIXES cocaine)

find_library(
    LIBCOCAINE_LIBRARY NAMES cocaine-core
    HINTS ${PC_LIBCOCAINE_LIBDIR} ${PC_LIBCOCAINE_LIBRARY_DIRS})

set(LIBCOCAINE_LIBRARIES ${LIBCOCAINE_LIBRARY})
set(LIBCOCAINE_INCLUDE_DIRS ${LIBCOCAINE_INCLUDE_DIR})

set(Cocaine_VERSION_MAJOR   0)
set(Cocaine_VERSION_MINOR   0)
set(Cocaine_VERSION_RELEASE 0)

file(STRINGS "${LIBCOCAINE_INCLUDE_DIR}/cocaine/config.hpp" _cocaine_CONFIG_HPP_CONTENTS REGEX "#define COCAINE_VERSION_")

set(_Cocaine_VERSION_MAJOR_REGEX "  ([0-9]+)")
set(_Cocaine_VERSION_MINOR_REGEX "  ([0-9]+)")
set(_Cocaine_VERSION_RELEASE_REGEX "([0-9]+)")

foreach(v MAJOR MINOR RELEASE)
    if("${_cocaine_CONFIG_HPP_CONTENTS}" MATCHES "#define COCAINE_VERSION_${v} ${_Cocaine_VERSION_${v}_REGEX}")
        set(Cocaine_VERSION_${v} "${CMAKE_MATCH_1}")
    endif()
endforeach()

unset(_cocaine_CONFIG_HPP_CONTENTS)

# Check version.
set(Cocaine_VERSION "${Cocaine_VERSION_MAJOR}.${Cocaine_VERSION_MINOR}.${Cocaine_VERSION_RELEASE}")
set(LIBCOCAINE_VERSION ${Cocaine_VERSION})
message("-- Cocaine version: ${Cocaine_VERSION}")

# Handle the QUIETLY and REQUIRED arguments and set LIBCOCAINE_FOUND to TRUE if all listed variables
# are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Cocaine DEFAULT_MSG
    LIBCOCAINE_LIBRARY LIBCOCAINE_INCLUDE_DIR)

mark_as_advanced(LIBCOCAINE_INCLUDE_DIR LIBCOCAINE_LIBRARY)
