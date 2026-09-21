# cmake/okkhor-core.cmake
#
# How this project gets okkhor-core. The core stays an independent library: its
# sources are never copied into this repository and its CMakeLists is never
# edited from here.
#
# Three ways to provide it, in order of preference:
#
#   1. An installed CMake package:
#        cmake -B build -DOKKHOR_CORE_USE_PACKAGE=ON
#      Used when the core one day ships `find_package(okkhor CONFIG)`.
#
#   2. A local checkout, for developing the two side by side:
#        cmake -B build -DOKKHOR_CORE_SOURCE_DIR=E:/programming/okkhor-core
#
#   3. FetchContent from GitHub (the default), pinned to a commit.

include(FetchContent)

option(OKKHOR_CORE_USE_PACKAGE "Use an installed okkhor CMake package" OFF)
set(OKKHOR_CORE_SOURCE_DIR "" CACHE PATH "Local okkhor-core checkout to build against")
set(OKKHOR_CORE_REPOSITORY "https://github.com/tajultonim/okkhor-core.git"
    CACHE STRING "okkhor-core git repository")
set(OKKHOR_CORE_TAG "150feeefa7b3ab5ad90d22dacead3af0de983bfd"
    CACHE STRING "okkhor-core git tag or commit")

function(okkhor_windows_add_core)
  if(OKKHOR_CORE_USE_PACKAGE)
    find_package(okkhor CONFIG REQUIRED)
    message(STATUS "okkhor-core: installed package")
    return()
  endif()

  if(OKKHOR_CORE_SOURCE_DIR)
    message(STATUS "okkhor-core: local checkout at ${OKKHOR_CORE_SOURCE_DIR}")
    add_subdirectory(${OKKHOR_CORE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/okkhor-core EXCLUDE_FROM_ALL)
    set(OKKHOR_CORE_DATA_DIR "${OKKHOR_CORE_SOURCE_DIR}/data" PARENT_SCOPE)
    return()
  endif()

  message(STATUS "okkhor-core: FetchContent ${OKKHOR_CORE_REPOSITORY}@${OKKHOR_CORE_TAG}")
  # EXCLUDE_FROM_ALL keeps the core's own CLI and test targets out of our
  # build; it is only understood by CMake 3.28 and newer, and is harmless to
  # omit (those targets simply get built too).
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    set(_okkhor_core_exclude EXCLUDE_FROM_ALL)
  else()
    set(_okkhor_core_exclude "")
  endif()
  FetchContent_Declare(okkhor_core_src
    GIT_REPOSITORY ${OKKHOR_CORE_REPOSITORY}
    GIT_TAG        ${OKKHOR_CORE_TAG}
    GIT_SHALLOW    FALSE
    ${_okkhor_core_exclude}
  )
  FetchContent_MakeAvailable(okkhor_core_src)
  set(OKKHOR_CORE_DATA_DIR "${okkhor_core_src_SOURCE_DIR}/data" PARENT_SCOPE)
endfunction()
