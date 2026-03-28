cmake_minimum_required(VERSION 3.20)
project(reddish VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/FS)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(config_type Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config_type} config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/output)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/lib)
endforeach()

find_package(Threads REQUIRED)
add_library(reddish_core
    src/server.cpp
    src/socket.cpp
    src/error.cpp
    src/database.cpp
    src/resp.cpp
)

target_include_directories(reddish_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reddish_core PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(reddish_core PUBLIC ws2_32)
endif()

add_executable(reddish src/main.cpp)
target_link_libraries(reddish PRIVATE reddish_core)

set_target_properties(reddish PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
