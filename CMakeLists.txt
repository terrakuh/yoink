cmake_minimum_required(VERSION 3.22)

project(
  yoink
  HOMEPAGE_URL "https://github.com/terrakuh/yoink"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)

set(default_build_type Release)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type for ${PROJECT_NAME} to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE
      "${default_build_type}"
      CACHE STRING "Build type." FORCE
  )
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include(cmake/CPM.cmake)

cpmaddpackage(
  NAME
  Boost
  VERSION
  1.84.0
  URL
  https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_HASH
  SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
  OPTIONS
  "BOOST_ENABLE_CMAKE ON"
  "BOOST_INCLUDE_LIBRARIES asio\\\;json\\\;process" # Note the
  # escapes!
)
cpmaddpackage("gh:CLIUtils/CLI11@2.4.1")
cpmaddpackage("gh:jbeder/yaml-cpp#47cd2725d61e54719933b83ea51c64ad60c24066")


add_subdirectory(yoinkd)
