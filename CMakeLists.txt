cmake_minimum_required(VERSION 3.21)

project(okkhor-windows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Options
# ==============================================================================

option(
  OKKHOR_WINDOWS_ENABLE_LOGGING
  "Write okkhor-windows.log"
  OFF
)

option(
  OKKHOR_WINDOWS_STATIC_RUNTIME
  "Link the MSVC runtime statically"
  ON
)

option(
  OKKHOR_WINDOWS_BUILD_TESTS
  "Build platform-independent seam tests"
  OFF
)

# ==============================================================================
# MSVC Runtime
# ==============================================================================

if(MSVC AND OKKHOR_WINDOWS_STATIC_RUNTIME)
  set(
    CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

# ==============================================================================
# Global Build Configuration
# ==============================================================================

if(MSVC)

  # Common compiler settings
  add_compile_options(
    /utf-8
    /EHsc
  )

  # --------------------------------------------------------------------------
  # Release optimization
  #
  # /O2     Optimize for speed
  # /GL     Whole-program optimization
  # /Gy     Enable function-level linking
  # /Gw     Enable data-level linking
  # --------------------------------------------------------------------------

  add_compile_options(
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/GL>
    $<$<CONFIG:Release>:/Gy>
    $<$<CONFIG:Release>:/Gw>
  )

  # --------------------------------------------------------------------------
  # Release linker optimization
  #
  # /LTCG      Link-time code generation
  # /OPT:REF   Remove unreferenced functions/data
  # /OPT:ICF   Fold identical COMDATs
  # --------------------------------------------------------------------------

  add_link_options(
    $<$<CONFIG:Release>:/LTCG>
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
  )

else()

  # GCC / Clang
  add_compile_options(
    $<$<CONFIG:Release>:-O3>
    -Wall
    -Wextra
    -Wpedantic
  )

endif()

# ==============================================================================
# Okkhor Core
# ==============================================================================

set(
  OKKHOR_CORE_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/external/okkhor-core"
)

if(NOT EXISTS "${OKKHOR_CORE_SOURCE_DIR}/CMakeLists.txt")
  message(
    FATAL_ERROR
    "okkhor-core submodule was not found.\n"
    "Initialize it with:\n"
    "    git submodule update --init --recursive"
  )
endif()

add_subdirectory(
  "${OKKHOR_CORE_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/okkhor-core"
  EXCLUDE_FROM_ALL
)

# ==============================================================================
# Core Seam
# ==============================================================================

add_library(
  okkhor_seam
  STATIC
  src/core/engine_host.cpp
)

target_include_directories(
  okkhor_seam
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(
  okkhor_seam
  PUBLIC
  okkhor_core
)

if(MSVC)
  target_compile_options(
    okkhor_seam
    PRIVATE
    /W4
  )
endif()

# ==============================================================================
# TSF Text Service
# ==============================================================================

if(WIN32)

  add_library(
    okkhor_tsf
    SHARED

    src/app/configuration.cpp

    src/tsf/text_service.cpp
    src/tsf/edit_session.cpp

    src/util/log.cpp

    src/windows/class_factory.cpp
    src/windows/dllmain.cpp
    src/windows/module.cpp
    src/windows/registration.cpp
    src/windows/unicode.cpp

    src/windows/okkhor_tsf.def

    src/resources/okkhor.rc
  )

  set_target_properties(
    okkhor_tsf
    PROPERTIES
    OUTPUT_NAME "okkhor_tsf"
  )

  target_link_libraries(
    okkhor_tsf
    PRIVATE

    okkhor_seam

    ole32
    oleaut32
    uuid
    advapi32
    shlwapi
    shell32
  )

  target_compile_definitions(
    okkhor_tsf
    PRIVATE

    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )

  if(OKKHOR_WINDOWS_ENABLE_LOGGING)
    target_compile_definitions(
      okkhor_tsf
      PRIVATE
      OKKHOR_WINDOWS_LOGGING
    )
  endif()

  if(MSVC)
    target_compile_options(
      okkhor_tsf
      PRIVATE
      /W4
    )
  endif()

  # --------------------------------------------------------------------------
  # Installation
  # --------------------------------------------------------------------------

  install(
    TARGETS okkhor_tsf
    RUNTIME DESTINATION .
  )

  install(
    FILES
    scripts/install.ps1
    scripts/uninstall.ps1
    DESTINATION scripts
  )

endif()

# ==============================================================================
# Tests
# ==============================================================================

if(OKKHOR_WINDOWS_BUILD_TESTS)

  enable_testing()

  add_executable(
    engine_host_tests
    tests/engine_host_tests.cpp
  )

  target_link_libraries(
    engine_host_tests
    PRIVATE
    okkhor_seam
  )

  if(MSVC)
    target_compile_options(
      engine_host_tests
      PRIVATE
      /W4
    )
  endif()

  add_test(
    NAME engine_host
    COMMAND engine_host_tests
  )

endif()
