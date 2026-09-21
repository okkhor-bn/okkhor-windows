cmake_minimum_required(VERSION 3.21)
project(okkhor-windows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Logging is compiled in by default; turn it off for a release build so that
# no host application text can ever be written to disk.
option(OKKHOR_WINDOWS_ENABLE_LOGGING "Write okkhor-windows.log" ON)
option(OKKHOR_WINDOWS_STATIC_RUNTIME
       "Link the MSVC runtime statically so the DLL needs no redistributable" ON)
option(OKKHOR_WINDOWS_BUILD_TESTS "Build the platform-independent seam tests" ON)

# The runtime library must be chosen before okkhor-core is added, so that the
# core and this project agree.
if(MSVC AND OKKHOR_WINDOWS_STATIC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/okkhor-core.cmake)
okkhor_windows_add_core()

# ---- the core seam -----------------------------------------------------------
# Platform-independent: owns the okkhor::Engine and nothing Windows-specific.
add_library(okkhor_seam STATIC
  src/core/engine_host.cpp
)
target_include_directories(okkhor_seam PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(okkhor_seam PUBLIC okkhor_core)
if(MSVC)
  target_compile_options(okkhor_seam PRIVATE /W4 /utf-8 /EHsc)
else()
  target_compile_options(okkhor_seam PRIVATE -Wall -Wextra)
endif()

# ---- the TSF text service ----------------------------------------------------
if(WIN32)
  add_library(okkhor_tsf SHARED
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
  )
  set_target_properties(okkhor_tsf PROPERTIES OUTPUT_NAME "okkhor_tsf")
  target_link_libraries(okkhor_tsf PRIVATE okkhor_seam ole32 oleaut32 uuid advapi32 shlwapi shell32)
  target_compile_definitions(okkhor_tsf PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

  if(OKKHOR_WINDOWS_ENABLE_LOGGING)
    target_compile_definitions(okkhor_tsf PRIVATE OKKHOR_WINDOWS_LOGGING)
  endif()

  if(MSVC)
    target_compile_options(okkhor_tsf PRIVATE /W4 /utf-8 /EHsc)
  endif()

  # A text service is loaded by every application that uses it, so the data
  # directory has to sit next to the DLL -- the working directory is the host
  # application's and tells us nothing.
  if(OKKHOR_CORE_DATA_DIR)
    add_custom_command(TARGET okkhor_tsf POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${OKKHOR_CORE_DATA_DIR} $<TARGET_FILE_DIR:okkhor_tsf>/data)
  endif()

  install(TARGETS okkhor_tsf RUNTIME DESTINATION .)
  if(OKKHOR_CORE_DATA_DIR)
    install(DIRECTORY ${OKKHOR_CORE_DATA_DIR}/ DESTINATION data)
  endif()
  install(FILES scripts/install.ps1 scripts/uninstall.ps1 DESTINATION scripts)
endif()

# ---- tests -------------------------------------------------------------------
if(OKKHOR_WINDOWS_BUILD_TESTS)
  enable_testing()
  add_executable(engine_host_tests tests/engine_host_tests.cpp)
  target_link_libraries(engine_host_tests PRIVATE okkhor_seam)
  if(MSVC)
    target_compile_options(engine_host_tests PRIVATE /W4 /utf-8 /EHsc)
  endif()
  add_test(NAME engine_host COMMAND engine_host_tests ${OKKHOR_CORE_DATA_DIR})
  # okkhor-core registers its own tests as a side effect of being added as a
  # subdirectory, and with EXCLUDE_FROM_ALL their executables are not built
  # here, so run this project's tests with:  ctest -R engine_host
endif()
