cmake_minimum_required(VERSION 3.10)

project(sandbox)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(ignis)

set (SOURCE_FILES
    sandbox/main.c
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ignis/src/
)

target_link_libraries(${PROJECT_NAME}
    ignis
)
