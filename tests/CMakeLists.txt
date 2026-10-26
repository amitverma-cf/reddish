add_executable(reddish_tests
    database_tests.cpp
    client_tests.cpp
    wal_tests.cpp
    memory_tests.cpp
    error_tests.cpp
    main_tests.cpp
    resp_tests.cpp
    server_tests.cpp
    socket_tests.cpp
    ${catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp
)

target_include_directories(reddish_tests PRIVATE ${catch2_SOURCE_DIR}/extras)
target_link_libraries(reddish_tests PRIVATE reddish_core reddish_client)
add_dependencies(reddish_tests reddish)
target_compile_definitions(reddish_tests PRIVATE REDDISH_EXECUTABLE="$<TARGET_FILE:reddish>")

add_test(NAME reddish_tests COMMAND reddish_tests)
set_tests_properties(reddish_tests PROPERTIES TIMEOUT 120)
add_executable(reddish_allocation_tests allocation_tests.cpp)
target_link_libraries(reddish_allocation_tests PRIVATE reddish_core)
add_test(NAME allocation_failures COMMAND reddish_allocation_tests)
set_tests_properties(allocation_failures PROPERTIES TIMEOUT 60)
find_package(Python3 3.9 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME process_integration COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/integration.py $<TARGET_FILE:reddish>)
    set_tests_properties(process_integration PROPERTIES TIMEOUT 90)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_library(reddish_wal_faults SHARED wal_faults.cpp)
        target_link_libraries(reddish_wal_faults PRIVATE ${CMAKE_DL_LIBS})
        set_tests_properties(process_integration PROPERTIES ENVIRONMENT "REDDISH_FAULT_SHIM=$<TARGET_FILE:reddish_wal_faults>")
    endif()
endif()

add_test(NAME rejects_zero_port COMMAND $<TARGET_FILE:reddish> 0)
add_test(NAME rejects_multiple_ports COMMAND $<TARGET_FILE:reddish> 6379 6380)
set_tests_properties(rejects_multiple_ports PROPERTIES WILL_FAIL TRUE TIMEOUT 5)
add_test(NAME rejects_out_of_range_port COMMAND $<TARGET_FILE:reddish> 65536)
add_test(NAME rejects_missing_flag_value COMMAND $<TARGET_FILE:reddish> --wal-path)
add_test(NAME rejects_zero_max_keys COMMAND $<TARGET_FILE:reddish> --max-keys 0)
add_test(NAME rejects_invalid_wal_sync COMMAND $<TARGET_FILE:reddish> --wal-sync bad)
add_test(NAME rejects_missing_wal_sync COMMAND $<TARGET_FILE:reddish> --wal-sync)
add_test(NAME rejects_invalid_max_keys COMMAND $<TARGET_FILE:reddish> --max-keys bad)
add_test(NAME rejects_removed_snapshot_flag COMMAND $<TARGET_FILE:reddish> --dump-interval 0)
add_test(NAME rejects_zero_max_memory COMMAND $<TARGET_FILE:reddish> --max-memory 0)
add_test(NAME rejects_missing_max_memory COMMAND $<TARGET_FILE:reddish> --max-memory)
add_test(NAME rejects_compaction_without_wal COMMAND $<TARGET_FILE:reddish> --compact-wal --no-wal)

set_tests_properties(rejects_zero_port rejects_out_of_range_port rejects_missing_flag_value
                     rejects_zero_max_keys rejects_invalid_wal_sync rejects_missing_wal_sync
                     rejects_invalid_max_keys rejects_removed_snapshot_flag rejects_zero_max_memory
                     rejects_missing_max_memory rejects_compaction_without_wal PROPERTIES WILL_FAIL TRUE)
