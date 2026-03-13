cmake_minimum_required(VERSION 3.16)
project(fluent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /DNDEBUG")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    endif()
endif()

set(SOURCES
    src/main.cpp
    src/value.cpp
    src/core.cpp
    src/exec.cpp
    src/vars.cpp
    src/control.cpp
    src/io.cpp
    src/system.cpp
)

add_executable(fluent ${SOURCES})
target_include_directories(fluent PRIVATE include)

# Link filesystem library (needed for GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(fluent stdc++fs)
endif()

# Threads
find_package(Threads REQUIRED)
target_link_libraries(fluent Threads::Threads)

# Windows specific
if(WIN32)
    target_link_libraries(fluent ws2_32)
endif()

# Install
install(TARGETS fluent DESTINATION bin)

message(STATUS "FLUENT ${PROJECT_VERSION} configured")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
