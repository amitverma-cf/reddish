add_executable(reddish_tests
    database_tests.cpp
    error_tests.cpp
    resp_tests.cpp
    server_tests.cpp
    ${catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp
)

target_include_directories(reddish_tests PRIVATE ${catch2_SOURCE_DIR}/extras)
target_link_libraries(reddish_tests PRIVATE reddish_core)

add_test(NAME reddish_tests COMMAND reddish_tests)
