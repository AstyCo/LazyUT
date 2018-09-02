##
## PROJECT OUTPUT DIRECTORIES
##
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

##
## CONFIGURATION
##
set(LAZYUT_TARGET_NAME                  ${PROJECT_NAME})
set(LAZYUT_CONFIG_INSTALL_DIR           "lib/cmake/${PROJECT_NAME}"
  CACHE INTERNAL "")
set(LAZYUT_INCLUDE_INSTALL_DIR          "include")
set(LAZYUT_TARGETS_EXPORT_NAME          "${PROJECT_NAME}Targets")
set(LAZYUT_CMAKE_CONFIG_TEMPLATE        "cmake/config.cmake.in")
set(LAZYUT_CMAKE_CONFIG_DIR             "${CMAKE_CURRENT_BINARY_DIR}")
set(LAZYUT_CMAKE_VERSION_CONFIG_FILE    "${LAZYUT_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(LAZYUT_CMAKE_PROJECT_CONFIG_FILE    "${LAZYUT_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")
set(LAZYUT_CMAKE_PROJECT_TARGETS_FILE   "${LAZYUT_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Targets.cmake")

##
## OPTIONS
##
option(BUILD_TESTS          "Build tests"                                                   ON)