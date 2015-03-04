# - Try to find Blackhole
# Once done this will define:
#  Blackhole_FOUND        - system has Blackhole
#  Blackhole_INCLUDE_DIRS - the Blackhole include directories

find_package(PkgConfig)

find_path(BLACKHOLE_INCLUDE_DIR
    NAMES blackhole/blackhole.hpp
    HINTS ${PC_BLACKHOLE_INCLUDEDIR}
          ${PC_BLACKHOLE_INCLUDE_DIRS}
    PATH_SUFFIXES blackhole
)

set(BLACKHOLE_INCLUDE_DIRS ${BLACKHOLE_INCLUDE_DIR})

# Handle the QUIETLY and REQUIRED arguments and set LIBCOCAINE_FOUND to TRUE if all listed variables
# are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Blackhole DEFAULT_MSG
    BLACKHOLE_INCLUDE_DIR)

mark_as_advanced(BLACKHOLE_INCLUDE_DIR)
