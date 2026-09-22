cmake_minimum_required(VERSION 3.24)
project(reaper_mcp VERSION 0.1.0 LANGUAGES CXX)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The extension targets 64-bit hosts")
endif()
if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The extension currently targets Windows x64 and Linux x86_64")
endif()
include(FetchContent)
FetchContent_Declare(reaper_sdk GIT_REPOSITORY https://github.com/justinfrankel/reaper-sdk.git GIT_TAG 08e885d14c87c2973ec301d220ff9fe6b6a7cbeb)
FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG 55f93686c01528224f448c19128836e7df245f72)
FetchContent_MakeAvailable(reaper_sdk json)
if(NOT WIN32)
  # Outside Win32, reaper_plugin.h includes "../WDL/swell/swell.h" for the
  # windowing types it declares. The SDK ships without WDL and expects it merged
  # in as a sibling of sdk/, which is what the link below provides.
  FetchContent_Declare(wdl GIT_REPOSITORY https://github.com/justinfrankel/WDL.git GIT_TAG 8f4d783de745126ac8c201455dc30818c8613324)
  FetchContent_MakeAvailable(wdl)
  if(NOT EXISTS ${reaper_sdk_SOURCE_DIR}/WDL)
    file(CREATE_LINK ${wdl_SOURCE_DIR}/WDL ${reaper_sdk_SOURCE_DIR}/WDL SYMBOLIC COPY_ON_ERROR)
  endif()
endif()

# Platform wiring is identical for the extension and every test binary, so it
# lives in one place rather than being repeated per target.
function(rmcp_configure target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
  if(WIN32)
    target_link_libraries(${target} PRIVATE ws2_32)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  else()
    # SWELL emulates the Win32 headers and, like them, defines min/max as macros
    # unless asked not to. Those macros break every std::min/std::max call.
    target_compile_definitions(${target} PRIVATE WDL_NO_DEFINE_MINMAX NOMINMAX)
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /EHsc /utf-8)
  else()
    # The sources deliberately pack statements onto single lines, which GCC and
    # Clang read as misleading indentation; the warning says nothing about
    # correctness here and reformatting is out of scope for a platform port.
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-misleading-indentation)
  endif()
endfunction()

add_library(reaper_mcp SHARED src/main.cpp)
target_include_directories(reaper_mcp PRIVATE include)
target_include_directories(reaper_mcp SYSTEM PRIVATE ${reaper_sdk_SOURCE_DIR}/sdk)
rmcp_configure(reaper_mcp)
# REAPER loads reaper_*.dll on Windows and reaper_*.so on Linux, so the target
# name is the file name and the default "lib" prefix must not be added. Only the
# entry point is exported; everything else stays internal to the module.
set_target_properties(reaper_mcp PROPERTIES PREFIX ""
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS reaper_mcp RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES ${reaper_sdk_SOURCE_DIR}/sdk/LICENSE DESTINATION licenses RENAME REAPER-SDK.txt)
install(FILES ${json_SOURCE_DIR}/LICENSE.MIT DESTINATION licenses RENAME nlohmann-json.txt)
include(CTest)
if(BUILD_TESTING)
  add_executable(platform_test tests/platform_test.cpp)
  target_include_directories(platform_test PRIVATE include)
  rmcp_configure(platform_test)
  add_test(NAME platform_primitives COMMAND platform_test)

  add_executable(bridge_test tests/bridge_test.cpp)
  target_include_directories(bridge_test PRIVATE include)
  rmcp_configure(bridge_test)
  add_test(NAME bridge_security COMMAND bridge_test)

  add_executable(adapter_test tests/adapter_test.cpp)
  target_include_directories(adapter_test PRIVATE include)
  target_include_directories(adapter_test SYSTEM PRIVATE ${reaper_sdk_SOURCE_DIR}/sdk)
  rmcp_configure(adapter_test)
  add_test(NAME adapter_semantics COMMAND adapter_test)

  add_executable(items_test tests/items_test.cpp)
  target_include_directories(items_test PRIVATE include)
  target_include_directories(items_test SYSTEM PRIVATE ${reaper_sdk_SOURCE_DIR}/sdk)
  rmcp_configure(items_test)
  add_test(NAME item_semantics COMMAND items_test)

  add_executable(render_test tests/render_test.cpp)
  target_include_directories(render_test PRIVATE include)
  target_include_directories(render_test SYSTEM PRIVATE ${reaper_sdk_SOURCE_DIR}/sdk)
  rmcp_configure(render_test)
  add_test(NAME render_isolation COMMAND render_test)
endif()
