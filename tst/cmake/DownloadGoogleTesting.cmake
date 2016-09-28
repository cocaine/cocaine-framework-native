INCLUDE(Compiler)
INCLUDE(ExternalProject)

FUNCTION(DOWNLOAD_GOOGLE_TESTING)
    SET(GTEST_URL "https://github.com/google/googletest/archive/release-1.8.0.zip")
    SET(GTEST_URL_HASH "MD5=adfafc8512ab65fd3cf7955ef0100ff5")

    SET_DIRECTORY_PROPERTIES(properties EP_PREFIX "${CMAKE_BINARY_DIR}/foreign")
    ExternalProject_ADD(googletest
        URL ${GTEST_URL}
        URL_HASH ${GTEST_URL_HASH}
        SOURCE_DIR "${CMAKE_BINARY_DIR}/foreign/gtest"
        CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_CXX_FLAGS=-fPIC"
        INSTALL_COMMAND ""
    )
    ExternalProject_GET_PROPERTY(googletest SOURCE_DIR)
    ExternalProject_GET_PROPERTY(googletest BINARY_DIR)

    SET(GTEST_INCLUDE_DIR ${SOURCE_DIR}/googletest/include PARENT_SCOPE)
    SET(GMOCK_INCLUDE_DIR ${SOURCE_DIR}/googlemock/include PARENT_SCOPE)
    SET(GTEST_BINARY_DIR ${BINARY_DIR}/googlemock/gtest PARENT_SCOPE)
    SET(GMOCK_BINARY_DIR ${BINARY_DIR}/googlemock PARENT_SCOPE)
ENDFUNCTION()
