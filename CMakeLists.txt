cmake_minimum_required(VERSION 3.20) # FetchContent is available in 3.11+

project(rtplib-test VERSION 0.0.1)

#build compiler tool
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out)

set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 0817715923c9705e68994eb52ef9df3f6845beba) # The commit hash for 1.10.x. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

# config IDE ext 
include(CTest)
enable_testing()

add_subdirectory(rtplib)
add_subdirectory(${EXTERNAL_DIR}/opus-1.4)

add_library(libavformat STATIC IMPORTED)
add_library(libavutil STATIC IMPORTED)
add_library(libavcodec STATIC IMPORTED)
add_library(libavdevice STATIC IMPORTED)
add_library(libswresample STATIC IMPORTED)
add_library(libswscale STATIC IMPORTED)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  #target_compile_options(myApp PRIVATE /W4)
  set_property(TARGET libavformat PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/avformat.lib)
  set_property(TARGET libavutil PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/avutil.lib)
  set_property(TARGET libavcodec PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/avcodec.lib)
  set_property(TARGET libavdevice PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/avdevice.lib)
  set_property(TARGET libswresample PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/swresample.lib)
  set_property(TARGET libswscale PROPERTY IMPORTED_LOCATION ${EXTERNAL_DIR}/ffmpeg/lib/x64/swscale.lib)
  Set(FFMPEG_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/FFmpeg/lib/x64/)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  #target_compile_options(myApp PRIVATE -Wall -Wextra -Wpedantic)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # other macOS-specific flags for Clang
endif()

add_executable(${PROJECT_NAME}
  src/main.cc
  src/utils.h
  src/testing/ffmpeg-encoder.h
  src/testing/ffmpeg-reader.h
  src/testing/opus-encoder.h
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE 
  rtplib
  libavformat
  libavutil
  libavcodec
  libavdevice
  libswresample
  libswscale
  opus
  cpr::cpr
  nlohmann_json::nlohmann_json
)

target_include_directories(${PROJECT_NAME}
  SYSTEM PRIVATE
  cpr::cpr
  
  ${EXTERNAL_DIR}/FFmpeg/include
  ${EXTERNAL_DIR}/opus-1.4/include
)

Set(FFMPEG_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/FFmpeg/lib/x64/)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD  COMMAND ${CMAKE_COMMAND} -E copy_directory ${FFMPEG_DLL_DIR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/)
