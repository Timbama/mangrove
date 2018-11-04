# Copyright 2016 MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(MANGROVE)

find_package(libbsoncxx 3.4 REQUIRED)

find_package(libmongocxx 3.4 REQUIRED)

find_package(libbson-1.0 1.13.0 REQUIRED)

find_package(libmongoc-1.0 1.13.0 REQUIRED)

# Update these as needed.
# TODO: read from file
set(MANGROVE_VERSION_MAJOR 0)
set(MANGROVE_VERSION_MINOR 0)
set(MANGROVE_VERSION_PATCH 1)
set(MANGROVE_VERSION_EXTRA "-alpha")
set(MANGROVE_ABI_VERSION _noabi)

set(MANGROVE_VERSION ${MANGROVE_VERSION_MAJOR}.${MANGROVE_VERSION_MINOR}.${MANGROVE_VERSION_PATCH}${MANGROVE_VERSION_EXTRA})
set(MANGROVE_INLINE_NAMESPACE "v${MANGROVE_ABI_VERSION}")
set(MANGROVE_HEADER_INSTALL_DIR "include/mangrove/${MANGROVE_INLINE_NAMESPACE}" CACHE INTERNAL "")

add_subdirectory(config)

# auto-generate MANGROVE_CHILD macros for accessing nested fields in the query builder.
message(STATUS "Generating MANGROVE_CHILD preprocessor macros...")
execute_process(COMMAND ${CMAKE_SOURCE_DIR}/buildscripts/generate_macros.py ${CMAKE_SOURCE_DIR}/src/mangrove/mangrove_child_autogen.hpp)

set(mangrove_sources
   "model.cpp"
   "collection_wrapper.cpp"
)

include_directories(
    ${LIBBSONCXX_INCLUDE_DIRS}
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${BSON_INCLUDE_DIRS}
    ${LIBMONGOC_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/third_party/cereal/include
)

link_directories(
    ${LIBBSONCXX_LIBRARY_DIRS}
    ${LIBMONGOCXX_LIBRARY_DIRS}
    ${BSON_LIBRARY_DIRS}
    ${MONGOC_LIBRARY_DIRS}
)

add_library(mangrove_static STATIC
   ${mangrove_sources}
)

target_compile_definitions(mangrove_static
    PUBLIC MANGROVE_STATIC
    PRIVATE ${MONGOC_DEFINITIONS}
    PRIVATE ${BSON_DEFINITIONS}
)

set_target_properties(mangrove_static PROPERTIES
    OUTPUT_NAME mangrove
)

# Follow the boost convention to disambiguate the dll and static
# library names
if (WIN32)
    set_target_properties(mangrove_static PROPERTIES
        PREFIX lib
    )
endif()

add_library(mangrove SHARED
    ${mangrove_sources}
)

set_target_properties (mangrove PROPERTIES
    OUTPUT_NAME mangrove
    VERSION ${MANGROVE_VERSION}
    DEFINE_SYMBOL MANGROVE_EXPORTS
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    SOVERSION ${MANGROVE_ABI_VERSION}
)

generate_export_header (mangrove
    BASE_NAME MANGROVE
    EXPORT_MACRO_NAME MANGROVE_API
    NO_EXPORT_MACRO_NAME MANGROVE_PRIVATE
    EXPORT_FILE_NAME config/export.hpp
    STATIC_DEFINE MANGROVE_STATIC
)

set(mangrove_libs ${LIBBSONCXX_LIBRARIES} ${LIBMONGOCXX_LIBRARIES} ${MONGOC_LIBRARIES} ${BSON_LIBRARIES})

target_link_libraries(mangrove_static boson_static ${mangrove_libs})
target_link_libraries(mangrove PUBLIC boson PRIVATE ${mangrove_libs})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION ${MANGROVE_HEADER_INSTALL_DIR}
    COMPONENT dev
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
        REGEX ".+/test" EXCLUDE
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/config/export.hpp
    DESTINATION ${MANGROVE_HEADER_INSTALL_DIR}/mangrove/config
    COMPONENT dev
)

install(TARGETS
    mangrove
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT dev
)

install(TARGETS
    mangrove_static
    ARCHIVE DESTINATION lib COMPONENT dev
)

set(PACKAGE_INCLUDE_INSTALL_DIRS ${MANGROVE_HEADER_INSTALL_DIR})
set(PACKAGE_LIBRARY_INSTALL_DIRS lib)
set(PACKAGE_LIBRARIES mangrove)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  cmake/libmangrove-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/libmangrove-config.cmake
  INSTALL_DESTINATION lib/cmake/libmangrove-${MANGROVE_VERSION}
  PATH_VARS PACKAGE_INCLUDE_INSTALL_DIRS PACKAGE_LIBRARY_INSTALL_DIRS
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/libmangrove-config-version.cmake
  VERSION ${MANGROVE_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/libmangrove-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/libmangrove-config-version.cmake
  DESTINATION lib/cmake/libmangrove-${MANGROVE_VERSION}
)

add_subdirectory(test)
