add_executable(reddish_tests
    database_tests.cpp
    error_tests.cpp
    main_tests.cpp
    resp_tests.cpp
    server_tests.cpp
    ${catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp
)

target_include_directories(reddish_tests PRIVATE ${catch2_SOURCE_DIR}/extras)
target_link_libraries(reddish_tests PRIVATE reddish_core)
target_compile_definitions(reddish_tests PRIVATE REDDISH_EXECUTABLE="$<TARGET_FILE:reddish>")

add_test(NAME reddish_tests COMMAND reddish_tests)

add_test(NAME rejects_zero_port COMMAND $<TARGET_FILE:reddish> 0)
add_test(NAME rejects_out_of_range_port COMMAND $<TARGET_FILE:reddish> 65536)
add_test(NAME rejects_missing_flag_value COMMAND $<TARGET_FILE:reddish> --load-dump)
add_test(NAME rejects_zero_max_keys COMMAND $<TARGET_FILE:reddish> --max-keys 0)
add_test(NAME rejects_invalid_dump_interval COMMAND $<TARGET_FILE:reddish> --dump-interval bad)

set_tests_properties(rejects_zero_port rejects_out_of_range_port rejects_missing_flag_value
                     rejects_zero_max_keys rejects_invalid_dump_interval PROPERTIES WILL_FAIL TRUE)
