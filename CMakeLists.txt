cmake_minimum_required(VERSION 3.20)
project(reddish VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(reddish
    src/main.cpp
    src/resp.cpp
)
