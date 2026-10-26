cmake_minimum_required(VERSION 3.16)
project(cache_research CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
if (BUILD_TESTING)
    if (POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2)
endif()

add_library(cache_lib STATIC
    src/read_config.cpp
)

target_include_directories(cache_lib PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(cache_lib PUBLIC cxx_std_17)

add_executable(cache_sim
    src/main.cpp
)

target_link_libraries(cache_sim PRIVATE cache_lib)

add_executable(cache_research src/research.cpp)
target_link_libraries(cache_research PRIVATE cache_lib)

if (BUILD_TESTING)
  add_executable(
    cache_tests
    tests/lru_test.cpp
    tests/experiment_test.cpp
    tests/lfu_test.cpp
    tests/cache_level_test.cpp
    tests/ideal_test.cpp
    tests/two_q_test.cpp
    tests/adaptive_cache_test.cpp
    tests/workloads_test.cpp
    tests/research_test.cpp
  )
  target_link_libraries(
  cache_tests PRIVATE cache_lib
  GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(cache_tests)
  find_package(Python3 COMPONENTS Interpreter)
  if (Python3_Interpreter_FOUND)
    add_test(NAME research_report_tests
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/report_test.py")
    add_test(NAME research_smoke
             COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/research_smoke.py"
                     $<TARGET_FILE:cache_research> $<TARGET_FILE:cache_sim>
                     "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_research.py")
  endif()
endif()
