cmake_minimum_required(VERSION 3.16)

project(borlib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Features
set(BOR_CHECK ON)
set(BOR_STACKTRACE ON)

add_executable(borlib)

file(GLOB borlib_files CONFIGURE_DEPENDS "src/**/*.cpp" "src/*.cpp")
target_sources(borlib PRIVATE ${borlib_files})

# Uncomment to add link library
if(BOR_STACKTRACE)
    target_link_libraries(borlib PRIVATE -lstdc++exp)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOR_STACKTRACE=1")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOR_STACKTRACE=0")
endif()

if(BOR_CHECK)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOR_CHECK=1")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOR_CHECK=0")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -fsanitize=address")
    target_link_libraries(borlib PRIVATE -fsanitize=address)
endif()
