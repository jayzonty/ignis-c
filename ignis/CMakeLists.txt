cmake_minimum_required(VERSION 3.10)

project(ignis)

find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(${FREETYPE_INCLUDE_DIRS})
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(deps/glfw-3.3.8/)

set (SOURCE_FILES
    src/ignis.c
    src/ignis_math.c
    src/ignis_vector.c
    src/ignis_vulkan.c
)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${FREETYPE_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIR}
    deps/glfw-3.3.8/include/
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Vulkan_LIBRARY}
    freetype
    glfw
    ${CMAKE_DL_LIBS} 
    m
)
