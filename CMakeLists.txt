cmake_minimum_required(VERSION 3.16)
project(RTSP_PUSHER VERSION 1.0.0.0)

#打印project相关信息
message(STATUS "......CMAKE_PROJECT_NAME    = ${CMAKE_PROJECT_NAME}")
message(STATUS "......PROJECT_VERSION_MAJOR = ${PROJECT_VERSION_MAJOR}")
message(STATUS "......PROJECT_VERSION_MINOR = ${PROJECT_VERSION_MINOR}")
message(STATUS "......PROJECT_VERSION_PATCH = ${PROJECT_VERSION_PATCH}")
message(STATUS "......PROJECT_VERSION_TWEAK = ${PROJECT_VERSION_TWEAK}")

message(STATUS "......PROJECT_HOMEPAGE_URL  = ${PROJECT_HOMEPAGE_URL}")           #没有设置
message(STATUS "......CMAKE_PROJECT_DESCRIPTION = ${CMAKE_PROJECT_DESCRIPTION}")  #没有设置
message(STATUS "......PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
message(STATUS "......PROJECT_BINARY_DIR = ${PROJECT_BINARY_DIR}")
message(STATUS "......operation system: ${CMAKE_SYSTEM}")

#打印当前系统平台
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "current platform: Linux ")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Android")
    MESSAGE(STATUS "current platform: Android")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    MESSAGE(STATUS "current platform: Emscripten")
ELSE ()
    MESSAGE(STATUS "other platform: ${CMAKE_SYSTEM_NAME}")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

# 配置C++选项
# CMAKE_CXX_FLAGS 表示会将C++的参数传给编译器
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall")

set(ffmpeg_include_dir 3rdParty/ffmpeg-4.2.1/include)
set(ffmpeg_lib_dir 3rdParty/ffmpeg-4.2.1/lib)

# 增加子目录
add_subdirectory(log)

# 头文件查找目录
# 这样在main函数中 就可以直接写#include <demo_thread.h>，而不用写上完整的路径
include_directories(log)
include_directories(${ffmpeg_include_dir})

# 链接库查找目录，否则下面就不能直接使用avformat(而是需要加入对应的路径)
# 推荐使用这种方式来链接库：先引入库的目录，后面可以直接target库的名字
link_directories(${ffmpeg_lib_dir})

#增加可执行程序
aux_source_directory(. DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})

# 给可执行程序添加链接库
target_link_libraries(${PROJECT_NAME} log)

target_link_libraries(${PROJECT_NAME} pthread avfilter avformat avcodec avutil swresample swscale)
