cmake_minimum_required(VERSION 3.20)
project(reddish VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
add_executable(reddish
    src/main.cpp
    src/server.cpp
    src/socket.cpp
    src/database.cpp
    src/resp.cpp
)

target_include_directories(reddish PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reddish PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(reddish PRIVATE ws2_32)
endif()

set_target_properties(reddish PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output
)
