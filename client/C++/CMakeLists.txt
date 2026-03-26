cmake_minimum_required(VERSION 3.20)
project(reddish_client LANGUAGES CXX)

add_library(reddish_client src/client.cpp src/socket.cpp)
target_include_directories(reddish_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(reddish_client PUBLIC cxx_std_23)

if(WIN32)
    target_link_libraries(reddish_client PRIVATE ws2_32)
endif()
