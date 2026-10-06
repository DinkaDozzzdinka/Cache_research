cmake_minimum_required(VERSION 3.16)
project(cache_research CXX)

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