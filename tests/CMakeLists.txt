add_executable(reddish_tests
    database_tests.cpp
    error_tests.cpp
    resp_tests.cpp
)

target_link_libraries(reddish_tests PRIVATE reddish_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(reddish_tests)
