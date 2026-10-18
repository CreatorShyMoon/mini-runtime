add_executable(fuzz_cli fuzz_cli.cpp)
target_link_libraries(fuzz_cli PRIVATE ctr_core)
# Address/undefined sanitizers come from -DENABLE_ASAN/-DENABLE_UBSAN.
target_compile_options(fuzz_cli PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_cli PRIVATE -fsanitize=fuzzer)
