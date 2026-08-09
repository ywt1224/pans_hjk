cmake_minimum_required(VERSION 3.25)

#这几个能看出来是设置对应的编译器位置，事实上去对应的/usr/bin 目录也能看到对应确实有这两个
#TODO 但是后面的那个 CACHE FILEPATH 是干啥的呢
#其它的set是不是也能这么设置
set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "/usr/bin/gcc" CACHE FILEPATH "C++ compiler" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) #这个是干嘛的呢

#这个include里面的是啥，首先没有补全提示，应该就不是CMAKE内置的吧
project(pans VERSION 0.1.0 LANGUAGES CXX C)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

#这个应该就是设置词条对应的指代含义？以及默认选项？
option(PANS_BUILD_EXAMPLES "Build examples" ON)
option(PANS_BUILD_TESTS    "Build tests"    ON)
option(PANS_BUILD_TOOLS    "Build tools"    ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

cmake_host_system_information(RESULT CPU_NUMS QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "CPU NUMS on current server is: ${CPU_NUMS}")
math(EXPR JOBS "${CPU_NUMS} + ${CPU_NUMS} / 2")
set(CMAKE_BUILD_PARALLEL_LEVEL ${JOBS} CACHE STRING "并行编译线程数")
message(STATUS "Compiler default work jobs: ${CMAKE_BUILD_PARALLEL_LEVEL}")

include(cmake/pansOptions.cmake)

#触发pans目录下的 CMakeLists.txt 的编译方式
add_subdirectory(pans)
if(PANS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(PANS_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

