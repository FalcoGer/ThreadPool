cmake_minimum_required(VERSION 3.28)
project(ThreadPool VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_GENERATOR MATCHES "Ninja")
    # Ensure Ninja is at least version 1.11
    execute_process(
        COMMAND ninja --version
        OUTPUT_VARIABLE NINJA_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NINJA_VERSION VERSION_LESS "1.11")
        message(FATAL_ERROR "Ninja version >= 1.11 is required. Found: ${NINJA_VERSION}")
    endif()
elseif(CMAKE_GENERATOR MATCHES "Visual Studio ([1-9][0-9]*)")
    # Extract the version number from the generator name
    string(REGEX MATCH "Visual Studio ([0-9]+)" MS_VERSION "${CMAKE_GENERATOR}")

    # Check if the version is 17 or greater
    if(MS_VERSION LESS "17")
        message(STATUS "Visual Studio 17 or newer is required. Found: ${CMAKE_GENERATOR}, Version: ${MS_VERSION}")
    endif()
else()
    message(FATAL_ERROR "Generator must be Ninja (>= 1.11) or Visual Studio >= 17 (VS 2022).")
endif()

# Check the compiler
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
        message(FATAL_ERROR "Clang version >= 16.0 required!")
    endif()
    # check if CLANG_GCC_TOOLCHAIN_PATH is set
    if(DEFINED CLANG_GCC_TOOLCHAIN_PATH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${CLANG_GCC_TOOLCHAIN_PATH}")
    endif()
    if(DEFINED CLANG_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CLANG_CXX_COMPILER_FLAGS}")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC version >= 14.0 required!")
    endif()
    if(DEFINED GCC_CXX_COMPILER_FLAGS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_CXX_COMPILER_FLAGS}")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(MSVC_VERSION LESS 1734)
        message(FATAL_ERROR "MSVC version >= 17.34 is required. Found: ${MSVC_VERSION}")
    endif()
else()
    message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}!")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

add_library(ConcurrentResource)
target_sources(ConcurrentResource
    PUBLIC
      FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ConcurrentResource.cppm
)
set_target_properties(ConcurrentResource PROPERTIES CXX_SCAN_FOR_MODULES YES)

add_library(ThreadPoolModule)
target_sources(ThreadPoolModule
    PUBLIC
      FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ThreadPool-ETaskState.cppm
        ThreadPool-TaskPriority.cppm
        ThreadPool-TaskCanceled.cppm
        ThreadPool-TaskID.cppm
        ThreadPool-TaskTicket.cppm
        ThreadPool-ITask.cppm
        ThreadPool-ITaskUniquePtrPriorityComparator.cppm
        ThreadPool-Task.cppm
        ThreadPool.cppm
)
set_target_properties(ThreadPoolModule PROPERTIES CXX_SCAN_FOR_MODULES YES)
target_link_libraries(ThreadPoolModule PUBLIC ConcurrentResource)

set(SRC main.cpp)

# Function to create a target with specific sanitizer flags
function(add_sanitized_target name)
    add_executable(${name} ${SRC})
    target_link_libraries(${name} PRIVATE ThreadPoolModule)
    target_compile_options(${name} PRIVATE -g -fno-omit-frame-pointer ${ARGN})
    target_link_options(${name} PRIVATE -g -fno-omit-frame-pointer ${ARGN})
endfunction()

# ASan + UBSan
add_sanitized_target(sanitize_asan "-fsanitize=address,undefined")

# TSan only
add_sanitized_target(sanitize_tsan "-fsanitize=thread")

# MSan only
add_sanitized_target(sanitize_msan "-fsanitize=memory")

add_executable(ThreadPoolExample ${SRC})
target_link_libraries(ThreadPoolExample PRIVATE ThreadPoolModule)
