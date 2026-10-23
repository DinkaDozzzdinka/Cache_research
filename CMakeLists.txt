cmake_minimum_required(VERSION 3.16)
project(cache_research CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2)
endif()

# Библиотека обязана содержать src/config.cpp
add_library(cache_lib STATIC
    src/read_config.cpp
)

target_include_directories(cache_lib PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(cache_lib PUBLIC cxx_std_17)

# Исполняемый файл
add_executable(cache_sim
    src/main.cpp
)

# Обязательная линковка!
target_link_libraries(cache_sim PRIVATE cache_lib)

enable_testing()

add_executable(
    cache_tests
    tests/lru_test.cpp
    tests/experiment_test.cpp
    tests/lfu_test.cpp
)
target_link_libraries(
  cache_tests PRIVATE cache_lib
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cache_tests)