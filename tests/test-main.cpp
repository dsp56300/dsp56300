// This is for speeding up recompilation of test files. Catch2 is pretty slow
// to compiled and this helps with creating a cached object file for each
// recompilation.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
