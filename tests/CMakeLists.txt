FetchContent_MakeAvailable(Catch2)

add_executable(dsp56kemu_tests jitunittests.cpp test-main.cpp)
target_link_libraries(dsp56kemu_tests PRIVATE Catch2::Catch2 dsp56kEmu)

# Needed to find the catch.cmake module
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)

# includes the catch_discover_tests() function
include(Catch)

# Parses the catch2 tests and adds a CTest test for each.
catch_discover_tests(dsp56kemu_tests)
