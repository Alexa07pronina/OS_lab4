cmake_minimum_required(VERSION 3.10)

project(OS_lab4 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


find_package(Threads REQUIRED)

add_executable(parent
    src/parent.c
    src/shared.c
)

add_executable(child
    src/child.c
    src/filter.c
    src/shared.c
)

target_link_libraries(parent PRIVATE Threads::Threads)
target_link_libraries(child PRIVATE Threads::Threads)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(parent PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(child PRIVATE -Wall -Wextra -Wpedantic)
endif()
