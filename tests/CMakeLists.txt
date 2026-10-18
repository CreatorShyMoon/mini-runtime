include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ctr_tests
    test_cli.cpp
    test_config.cpp
    test_namespaces.cpp
)
target_link_libraries(ctr_tests PRIVATE ctr_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(ctr_tests)

add_test(
    NAME integration_rootfs
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/integration.sh $<TARGET_FILE:mini-ctr>
)
