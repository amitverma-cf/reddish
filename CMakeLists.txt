cmake_minimum_required(VERSION 3.20)
project(reddish VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(REDDISH_ENABLE_COVERAGE "Enable LLVM source coverage instrumentation" OFF)
option(REDDISH_BUILD_DOCS "Add the Doxygen documentation target when available" ON)

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

if(REDDISH_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "REDDISH_ENABLE_COVERAGE requires Clang")
    endif()

    target_compile_options(reddish_core PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(reddish PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(reddish_core INTERFACE -fprofile-instr-generate)
endif()

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
    FetchContent_GetProperties(Catch2)
    if(NOT catch2_POPULATED)
        FetchContent_Populate(Catch2)
    endif()
    add_subdirectory(tests)
    if(REDDISH_ENABLE_COVERAGE)
        target_link_options(reddish_tests PRIVATE -fprofile-instr-generate)
    endif()
endif()

if(REDDISH_BUILD_DOCS)
    find_package(Doxygen QUIET)

    if(DOXYGEN_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            doxygen-awesome-css
            URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.4.2.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(doxygen-awesome-css)
        FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

        set(REDDISH_DOC_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/.assets)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            @ONLY
        )

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-folder.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/folderclosed.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-folder.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/folderclosedd.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-folder-open.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/folderopen.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-folder-open.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/folderopend.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-file-code.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/doc.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-file-code.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/docd.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-plus.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/plus.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-plus.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/plusd.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-minus.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/minus.svg
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${REDDISH_DOC_ASSETS_DIR}/icons/lucide-minus.svg ${CMAKE_CURRENT_SOURCE_DIR}/docs/minusd.svg
            COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen.svg
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating Doxygen documentation"
            VERBATIM
        )
    else()
        message(STATUS "Doxygen not found: documentation target is unavailable")
    endif()
endif()
