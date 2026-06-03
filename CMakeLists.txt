cmake_minimum_required(VERSION 3.16)
project(emcast VERSION 1.0.0 LANGUAGES CXX)

# ---- Standard / options -----------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(EMCAST_BUILD_TESTS "Build the emcast test suite" ON)
option(EMCAST_BUILD_CLI   "Build the emcast command-line tool" ON)
option(EMCAST_BUILD_BENCH "Build the BER-vs-SNR benchmark tool" ON)

include(FetchContent)

# ---- Header-only third-party libraries (downloaded, no sub-targets) ---------
# The SOURCE_SUBDIR trick populates the repo without adding its own CMake targets.
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.21
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR  do-not-add-subdir)
FetchContent_Declare(drlibs
  GIT_REPOSITORY https://github.com/mackron/dr_libs.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR  do-not-add-subdir)
FetchContent_MakeAvailable(miniaudio drlibs)

# ---- Core library -----------------------------------------------------------
add_library(emcast STATIC
  src/types.cpp
  src/crc32.cpp
  src/reed_solomon.cpp
  src/frame.cpp
  src/goertzel.cpp
  src/modem_common.cpp
  src/bfsk.cpp
  src/ook.cpp
  src/psk.cpp
  src/mfsk.cpp
  src/awgn.cpp
  src/wav_channel.cpp
  src/impl_dr_wav.cpp
  src/audio_channel.cpp
  src/impl_miniaudio.cpp
  src/emcast.cpp)
add_library(emcast::emcast ALIAS emcast)

target_include_directories(emcast PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_include_directories(emcast PRIVATE ${miniaudio_SOURCE_DIR} ${drlibs_SOURCE_DIR})

if(MSVC)
  target_compile_definitions(emcast PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
  target_compile_options(emcast PRIVATE /W4)
else()
  target_compile_options(emcast PRIVATE -Wall -Wextra)
endif()

# Platform audio backends required by miniaudio. PUBLIC so consumers that link
# the static emcast library inherit them at final link time.
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(emcast PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
elseif(APPLE)
  target_link_libraries(emcast PUBLIC
    "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()

# ---- CLI11 (shared by the CLI and the benchmark) ----------------------------
if(EMCAST_BUILD_CLI OR EMCAST_BUILD_BENCH)
  FetchContent_Declare(CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(CLI11)
endif()

if(EMCAST_BUILD_CLI)
  add_executable(emcast_cli apps/cli/main.cpp)
  set_target_properties(emcast_cli PROPERTIES OUTPUT_NAME emcast)
  target_link_libraries(emcast_cli PRIVATE emcast::emcast CLI11::CLI11)
endif()

if(EMCAST_BUILD_BENCH)
  add_executable(emcast_bench bench/ber_sweep.cpp)
  target_link_libraries(emcast_bench PRIVATE emcast::emcast CLI11::CLI11)
endif()

# ---- Tests ------------------------------------------------------------------
if(EMCAST_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)

  add_executable(emcast_tests
    tests/test_crc.cpp
    tests/test_reed_solomon.cpp
    tests/test_frame.cpp
    tests/test_goertzel.cpp
    tests/test_roundtrip.cpp
    tests/test_schemes.cpp
    tests/test_robustness.cpp)
  target_link_libraries(emcast_tests PRIVATE emcast::emcast Catch2::Catch2WithMain)
  if(MSVC)
    target_compile_definitions(emcast_tests PRIVATE _USE_MATH_DEFINES)
  endif()
  catch_discover_tests(emcast_tests)
endif()

# ---- Install / packaging ----------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS emcast EXPORT emcastTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/emcast DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(EMCAST_BUILD_CLI)
  install(TARGETS emcast_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(EXPORT emcastTargets
  FILE emcastTargets.cmake
  NAMESPACE emcast::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emcast)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emcastConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/emcastConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emcast)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/emcastConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/emcastConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/emcastConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/emcast)

set(CPACK_PACKAGE_NAME emcast)
set(CPACK_PACKAGE_VENDOR "Bikebrainz")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Data-over-signal transmission toolkit")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_GENERATOR "ZIP")
include(CPack)
